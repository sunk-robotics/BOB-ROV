#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <system_error>
#include <utility>
#include <variant>

#include "sensors/aht20/error.hpp"
#include "sensors/aht20/measure_timeout.hpp"
#include "sensors/aht20/measurement.hpp"
#include "sensors/checksum/cr8.hpp"
#include "i2c/i2c_bus.hpp"


namespace sensors::aht20 
{

/// Non-blocking driver for the AHT20 I2C temperature and humidity sensor.
/** The driver is a state machine. After `create()`, call `recover_step()` repeatedly until it
 * reports `RecoveryProgress::recovered`; then `trigger_measurement()` and poll
 * `try_read_raw()`/`try_read()` until a value arrives. No call ever sleeps.
 *
 * Consecutive measurement failures are counted internally; after `kFailureThreshold` of them the
 * driver drops back into recovery on its own. Callers can also force that with
 * `request_recovery()`. */
class Driver
{
  using SteadyClock = std::chrono::steady_clock;

 private:
  struct AwaitingReset { SteadyClock::time_point not_before; };
  struct AwaitingResetCompletion { SteadyClock::time_point not_before; };
  struct AwaitingCalibration { SteadyClock::time_point not_before; };
  struct Idle {};
  struct Measuring { std::chrono::steady_clock::time_point triggered_at; };

  using LifecycleState = std::variant<AwaitingReset, AwaitingResetCompletion, AwaitingCalibration, Idle, Measuring>;

 public:

  enum class RecoveryProgress
  {
    /// The sensor is initialized and ready for measurements.
    recovered,
    /// Still waiting on a sensor delay or about to take the next step.
    in_progress
  };

  Driver(const Driver&) = delete;
  Driver& operator=(const Driver&) = delete;

  /// Creates a driver in the recovery state. Performs no bus traffic.
  /** @param bus I2C bus used to communicate with the sensor. Must not be null.
   *  @param measure_timeout Maximum time `try_read_raw()` waits for a triggered measurement.
   *  @return The driver, or `ErrorCode::null_bus`. */
  [[nodiscard]] static std::expected<std::unique_ptr<Driver>, std::error_code> create(
      std::shared_ptr<I2cBus> bus,
      MeasureTimeout measure_timeout
  ) noexcept
  {
    if (!bus)
      return std::unexpected(make_error_code(ErrorCode::null_bus));
    return std::unique_ptr<Driver>(new Driver(std::move(bus), measure_timeout));
  }

  /// True once the sensor is initialized (whether or not a measurement is in flight).
  [[nodiscard]] bool is_ready() const noexcept
  {
    return std::holds_alternative<Idle>(state_) || std::holds_alternative<Measuring>(state_);
  }

  /// Abandons any measurement and restarts the reset/initialization sequence immediately.
  void request_recovery() noexcept
  {
    state_ = AwaitingReset{SteadyClock::now()};
    consecutive_failures_ = 0;
  }

  /// Advances the reset/initialization sequence by at most one step.
  /** @return `recovered` once the sensor is ready, `in_progress` while waiting, or the error that
   * made this attempt fail (the sequence restarts after `kRetryBackoff`). */
  [[nodiscard]] std::expected<RecoveryProgress, std::error_code> recover_step() noexcept
  {
    const auto now = SteadyClock::now();

    if (const auto* awaiting_reset = std::get_if<AwaitingReset>(&state_)) {
      if (now < awaiting_reset->not_before)
        return RecoveryProgress::in_progress;
      if (auto written = bus_->write(kI2cAddress, kSoftResetMessage); !written)
        return fail_recovery(now, written.error());
      state_ = AwaitingResetCompletion{now + kSoftResetTime};
      return RecoveryProgress::in_progress;
    }

    if (const auto* awaiting_completion = std::get_if<AwaitingResetCompletion>(&state_)) {
      if (now < awaiting_completion->not_before)
        return RecoveryProgress::in_progress;
      const auto status = read_status();
      if (!status)
        return fail_recovery(now, status.error());
      if (is_calibrated(*status)) {
        state_ = Idle{};
        return RecoveryProgress::recovered;
      }
      if (auto written = bus_->write(kI2cAddress, kInitializationMessage); !written)
        return fail_recovery(now, written.error());
      state_ = AwaitingCalibration{now + kCalibrationTime};
      return RecoveryProgress::in_progress;
    }

    if (const auto* awaiting_calibration = std::get_if<AwaitingCalibration>(&state_)) {
      if (now < awaiting_calibration->not_before)
        return RecoveryProgress::in_progress;
      const auto status = read_status();
      if (!status)
        return fail_recovery(now, status.error());
      if (is_calibrated(*status)) {
        state_ = Idle{};
        return RecoveryProgress::recovered;
      }
      return fail_recovery(now, make_error_code(ErrorCode::initialization_failed));
    }

    return RecoveryProgress::recovered;  // Idle or Measuring
  }

  /// Starts a conversion. Only valid when the sensor is ready and no measurement is in flight.
  [[nodiscard]] std::expected<void, std::error_code> trigger_measurement() noexcept
  {
    if (std::holds_alternative<Measuring>(state_))
      return std::unexpected(make_error_code(ErrorCode::measurement_already_in_progress));
    if (!std::holds_alternative<Idle>(state_))
      return std::unexpected(make_error_code(ErrorCode::not_ready));

    if (auto written = bus_->write(kI2cAddress, kTriggerMeasurementMessage); !written)
      return record_measurement_failure(written.error());

    // Stamp after the write returns: the sensor has been converting for at least this long.
    state_ = Measuring{SteadyClock::now()};
    return {};
  }

  /// Polls for the result of a triggered measurement.
  /** @return The measurement, `std::nullopt` if it is not ready yet, or an error. Any error ends
   * the measurement (the driver returns to idle, or to recovery once the failure threshold is
   * reached). A successful read returns the driver to idle. */
  [[nodiscard]] std::expected<std::optional<RawMeasurement>, std::error_code>
  try_read_raw() noexcept
  {
    const auto* measuring = std::get_if<Measuring>(&state_);
    if (measuring -# me llamas?== nullptr)
      return std::unexpected(make_error_code(ErrorCode::no_measurement_in_progress));
    const SteadyClock::time_point triggered_at = measuring->triggered_at;

    // Still converting: don't touch the bus.
    if (SteadyClock::now() - triggered_at < MeasureTimeout::kMinimumConversionTime)
      return std::optional<RawMeasurement>{};

    std::array<uint8_t, 7> response{};
    if (auto read_result = bus_->read(kI2cAddress, response); !read_result)
      return record_measurement_failure(read_result.error());

    const std::span<const uint8_t, 7> response_view{response};

    if ((response_view[0] & kMeasurementBusyMask) != 0) {
      // Checked after a read so that there is always at least one attempt.
      if (SteadyClock::now() - triggered_at >= measure_timeout_.duration())
        return record_measurement_failure(make_error_code(ErrorCode::measurement_timed_out));
      return std::optional<RawMeasurement>{};
    }

    if (kCrc8.calculate(response_view.first<6>()) != response_view[6])
      return record_measurement_failure(make_error_code(ErrorCode::crc_mismatch));

    state_ = Idle{};
    consecutive_failures_ = 0;
    return std::optional{RawMeasurement::from_data(response_view.subspan<1, 5>())};
  }

  /// Same as `try_read_raw()`, with the result converted to physical units.
  [[nodiscard]] std::expected<std::optional<Measurement>, std::error_code> try_read() noexcept
  {
    return try_read_raw().transform([](std::optional<RawMeasurement> maybe_raw) {
      return maybe_raw.transform(Measurement::from_raw);
    });
  }

 private:
  explicit Driver(std::shared_ptr<I2cBus> bus, MeasureTimeout measure_timeout)
      : bus_(std::move(bus)),
        measure_timeout_(measure_timeout),
        state_(AwaitingReset{SteadyClock::now() + kPowerOnDelay})
  {}

  [[nodiscard]] std::expected<uint8_t, std::error_code> read_status() noexcept
  {
    std::array<uint8_t, 1> status_buffer{};
    if (auto transferred = bus_->transfer(kI2cAddress, kStatusCommandMessage, status_buffer);
        !transferred)
      return std::unexpected(transferred.error());
    return status_buffer[0];
  }

  [[nodiscard]] static constexpr bool is_calibrated(uint8_t status) noexcept
  {
    return (status & kCalibrationEnabledMask) != 0;
  }

  std::unexpected<std::error_code> fail_recovery(
      SteadyClock::time_point now,
      std::error_code error
  ) noexcept
  {
    state_ = AwaitingReset{now + kRetryBackoff};
    return std::unexpected(error);
  }

  /// Ends any in-flight measurement, counting the failure toward the recovery threshold.
  std::unexpected<std::error_code> record_measurement_failure(std::error_code error) noexcept
  {
    if (++consecutive_failures_ >= kFailureThreshold)
      request_recovery();
    else
      state_ = Idle{};
    return std::unexpected(error);
  }

  static constexpr uint8_t kI2cAddress = 0x38;
  static constexpr std::array<uint8_t, 1> kStatusCommandMessage{0x71};
  static constexpr std::array<uint8_t, 1> kSoftResetMessage{0xBA};
  static constexpr std::array<uint8_t, 3> kInitializationMessage{0xBE, 0x08, 0x00};
  static constexpr std::array<uint8_t, 3> kTriggerMeasurementMessage{0xAC, 0x33, 0x00};
  static constexpr uint8_t kCalibrationEnabledMask = 0x08;
  static constexpr uint8_t kMeasurementBusyMask = 0x80;
  static constexpr Crc8 kCrc8{0x31, 0xFF};

  static constexpr uint8_t kFailureThreshold = 5;
  static constexpr SteadyClock::duration kPowerOnDelay = std::chrono::milliseconds(40);
  static constexpr SteadyClock::duration kSoftResetTime = std::chrono::milliseconds(20);
  static constexpr SteadyClock::duration kCalibrationTime = std::chrono::milliseconds(10);
  static constexpr SteadyClock::duration kRetryBackoff = std::chrono::seconds(1);

  std::shared_ptr<I2cBus> bus_;
  MeasureTimeout measure_timeout_;
  LifecycleState state_;
  uint8_t consecutive_failures_ = 0;
};

}
