#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <system_error>
#include <utility>
#include <variant>

#include <i2c/i2c_bus.hpp>
#include "sensors/ld/calibration.hpp"
#include "sensors/ld/error.hpp"
#include "sensors/ld/measurement.hpp"

namespace sensors::ld
{

/// Non-blocking driver for the Keller LD I2C pressure and temperature sensor.
/** The driver is a state machine. After `create()`, call `recover_step()` repeatedly until it
 * reports `RecoveryProgress::recovered` (it is reading the sensor's calibration registers);
 * then `trigger_measurement()` and poll `try_read_raw()`/`try_read()` until a value arrives. No
 * call evetarget_link_libraries(temp_humid_node PRIVATE
  sensors_aht20
  rclcpp::rclcpp
  rclcpp_components::component
  ${sensor_msgs_TARGETS})r sleeps. `next_action_time()` says when the next call will have something to do.
 *
 * Consecutive measurement failures are counted internally; after `kFailureThreshold` of them the
 * driver reloads its calibration on its own. Callers can also force that with
 * `request_recovery()`. */
class Driver
{
 public:
  using SteadyClock = std::chrono::steady_clock;

 private:
  struct CalibrationLoadProgress
  {
    std::array<uint16_t, Calibration::kRegisterCount> words{};
    std::size_t words_loaded = 0;
  };

  /// The next register address is due to be written.
  struct RequestingRegister
  {
    CalibrationLoadProgress progress;
    SteadyClock::time_point not_before;
  };
  /// A register address was written; its contents can be read once `not_before` passes.
  struct AwaitingRegister
  {
    CalibrationLoadProgress progress;
    SteadyClock::time_point not_before;
  };
  struct Idle
  {
    Calibration calibration;
  };
  struct Measuring
  {
    Calibration calibration;
    SteadyClock::time_point triggered_at;
  };

  using LifecycleState = std::variant<RequestingRegister, AwaitingRegister, Idle, Measuring>;

 public:
  enum class RecoveryProgress
  {
    /// The calibration is loaded and the sensor is ready for measurements.
    recovered,
    /// Still waiting on a sensor delay or about to take the next step.
    in_progress
  };

  Driver(const Driver&) = delete;
  Driver& operator=(const Driver&) = delete;

  /// Creates a driver in the recovery state. Performs no bus traffic.
  /** @return The driver, or `ErrorCode::null_bus`. */
  [[nodiscard]] static std::expected<std::unique_ptr<Driver>, std::error_code> create(
      std::shared_ptr<I2cBus> bus
  ) noexcept
  {
    if (!bus)
      return std::unexpected(make_error_code(ErrorCode::null_bus));
    return std::unique_ptr<Driver>(new Driver(std::move(bus)));
  }

  /// True once the calibration is loaded (whether or not a measurement is in flight).
  [[nodiscard]] bool is_ready() const noexcept
  {
    return std::holds_alternative<Idle>(state_) || std::holds_alternative<Measuring>(state_);
  }

  /// The calibration read from the sensor, or `nullptr` while still loading it.
  [[nodiscard]] const Calibration* calibration() const noexcept
  {
    if (const auto* idle = std::get_if<Idle>(&state_))
      return &idle->calibration;

    if (const auto* measuring = std::get_if<Measuring>(&state_))
      return &measuring->calibration;

    return nullptr;
  }

  /// The earliest time at which `recover_step()` or `try_read_raw()` will do anything.
  /** Already in the past (or "now") if the driver can act immediately. Lets callers sleep or
   * schedule a timer precisely instead of polling. */
  [[nodiscard]] SteadyClock::time_point next_action_time() const noexcept
  {
    if (const auto* requesting = std::get_if<RequestingRegister>(&state_))
      return requesting->not_before;
    if (const auto* awaiting = std::get_if<AwaitingRegister>(&state_))
      return awaiting->not_before;
    if (const auto* measuring = std::get_if<Measuring>(&state_))
      return measuring->triggered_at + kConversionTime;
    return SteadyClock::now();
  }

  /// Abandons any measurement and restarts loading the calibration immediately.
  void request_recovery() noexcept
  {
    state_ = RequestingRegister{CalibrationLoadProgress{}, SteadyClock::now()};
    consecutive_failures_ = 0;
  }

  /// Advances calibration loading by at most one bus transaction.
  /** @return `recovered` once the sensor is ready, `in_progress` while waiting, or the error that
   * made this attempt fail (loading restarts after `kRetryBackoff`). */
  [[nodiscard]] std::expected<RecoveryProgress, std::error_code> recover_step() noexcept
  {
    const auto now = SteadyClock::now();

    if (const auto* requesting = std::get_if<RequestingRegister>(&state_)) {
      if (now < requesting->not_before)
        return RecoveryProgress::in_progress;

      const CalibrationLoadProgress progress = requesting->progress;
      const std::array<uint8_t, 1> request{kCalibrationRegisterAddresses[progress.words_loaded]};
      if (auto written = bus_->write(kI2cAddress, request); !written)
        return fail_recovery(now, written.error());

      state_ = AwaitingRegister{progress, now + kRegisterAccessTime};
      return RecoveryProgress::in_progress;
    }

    if (const auto* awaiting = std::get_if<AwaitingRegister>(&state_)) {
      if (now < awaiting->not_before)
        return RecoveryProgress::in_progress;

      CalibrationLoadProgress progress = awaiting->progress;
      const auto word = read_register_word();
      if (!word)
        return fail_recovery(now, word.error());

      progress.words[progress.words_loaded] = *word;
      ++progress.words_loaded;

      if (progress.words_loaded < progress.words.size()) {
        state_ = RequestingRegister{progress, now};
        return RecoveryProgress::in_progress;
      }

      const auto loaded_calibration = Calibration::from_register_words(progress.words);
      if (!loaded_calibration)
        return fail_recovery(now, loaded_calibration.error());

      state_ = Idle{*loaded_calibration};
      consecutive_failures_ = 0;
      return RecoveryProgress::recovered;
    }

    return RecoveryProgress::recovered;  // Idle or Measuring
  }

  /// Starts a conversion. Only valid when the sensor is ready and no measurement is in flight.
  [[nodiscard]] std::expected<void, std::error_code> trigger_measurement() noexcept
  {
    if (std::holds_alternative<Measuring>(state_))
      return std::unexpected(make_error_code(ErrorCode::measurement_already_in_progress));

    const auto* idle = std::get_if<Idle>(&state_);
    if (idle == nullptr)
      return std::unexpected(make_error_code(ErrorCode::not_ready));
    const Calibration loaded_calibration = idle->calibration;

    if (auto written = bus_->write(kI2cAddress, kRequestMeasurementMessage); !written)
      return record_measurement_failure(written.error());

    // Stamp after the write returns: the sensor has been converting for at least this long.
    state_ = Measuring{loaded_calibration, SteadyClock::now()};
    return {};
  }

  /// Polls for the result of a triggered measurement.
  /** @return The measurement, `std::nullopt` if the conversion time has not elapsed yet, or an
   * error. Once the conversion time has elapsed the sensor must report completion, so a still-busy
   * sensor is `ErrorCode::response_timed_out`. Any error ends the measurement (the driver returns
   * to idle, or to recovery once the failure threshold is reached). A successful read returns the
   * driver to idle. */
  [[nodiscard]] std::expected<std::optional<RawMeasurement>, std::error_code>
  try_read_raw() noexcept
  {
    const auto* measuring = std::get_if<Measuring>(&state_);
    if (measuring == nullptr)
      return std::unexpected(make_error_code(ErrorCode::no_measurement_in_progress));

    // Still converting: don't touch the bus.
    if (SteadyClock::now() - measuring->triggered_at < kConversionTime)
      return std::optional<RawMeasurement>{};
    const Calibration loaded_calibration = measuring->calibration;

    std::array<uint8_t, 5> response{};
    if (auto read_result = bus_->read(kI2cAddress, response); !read_result)
      return record_measurement_failure(read_result.error());

    if (auto status = check_status_byte(response[0]); !status)
      return record_measurement_failure(status.error());

    state_ = Idle{loaded_calibration};
    consecutive_failures_ = 0;
    return std::optional{
        RawMeasurement::from_data(std::span<const uint8_t, 5>{response}.subspan<1, 4>())
    };
  }

  /// Same as `try_read_raw()`, with the result converted to physical units.
  [[nodiscard]] std::expected<std::optional<Measurement>, std::error_code> try_read() noexcept
  {
    const auto raw_result = try_read_raw();
    if (!raw_result)
      return std::unexpected(raw_result.error());
    if (!*raw_result)
      return std::optional<Measurement>{};

    // A successful read leaves the driver idle, so the calibration is available.
    const auto loaded_calibration = calibration();
    if (!loaded_calibration)
      return std::unexpected(make_error_code(ErrorCode::not_ready));

    return std::optional{
        Measurement::from_raw(**raw_result, loaded_calibration->pressure_range())
    };
  }

 private:
  explicit Driver(std::shared_ptr<I2cBus> bus)
      : bus_(std::move(bus)),
        state_(RequestingRegister{CalibrationLoadProgress{}, SteadyClock::now()})
  {}

  /// Reads the three-byte response to a register request: status byte, then a big-endian word.
  [[nodiscard]] std::expected<uint16_t, std::error_code> read_register_word() noexcept
  {
    std::array<uint8_t, 3> response{};
    if (auto read_result = bus_->read(kI2cAddress, response); !read_result)
      return std::unexpected(read_result.error());
    if (auto status = check_status_byte(response[0]); !status)
      return std::unexpected(status.error());
    return static_cast<uint16_t>((response[1] << 8) | response[2]);
  }

  [[nodiscard]] static std::expected<void, std::error_code> check_status_byte(
      uint8_t status_byte
  ) noexcept
  {
    if ((status_byte & kMemoryErrorMask) != 0)
      return std::unexpected(make_error_code(ErrorCode::memory_error));
    if ((status_byte & kBusyMask) != 0)
      return std::unexpected(make_error_code(ErrorCode::response_timed_out));
    return {};
  }

  std::unexpected<std::error_code> fail_recovery(
      SteadyClock::time_point now,
      std::error_code error
  ) noexcept
  {
    state_ = RequestingRegister{CalibrationLoadProgress{}, now + kRetryBackoff};
    return std::unexpected(error);
  }

  /// Ends any in-flight measurement, counting the failure toward the recovery threshold.
  std::unexpected<std::error_code> record_measurement_failure(std::error_code error) noexcept
  {
    if (++consecutive_failures_ >= kFailureThreshold) {
      request_recovery();
    } else if (const auto* measuring = std::get_if<Measuring>(&state_)) {
      const Calibration loaded_calibration = measuring->calibration;
      state_ = Idle{loaded_calibration};
    }
    return std::unexpected(error);
  }

  static constexpr uint8_t kI2cAddress = 0x40;
  static constexpr uint8_t kBusyMask = 0x20;
  static constexpr uint8_t kMemoryErrorMask = 0x04;
  static constexpr std::array<uint8_t, 1> kRequestMeasurementMessage{0xAC};

  /// Calibration registers in the order `Calibration::from_register_words()` expects.
  static constexpr std::array<uint8_t, Calibration::kRegisterCount> kCalibrationRegisterAddresses{
      0x12, 0x13, 0x14, 0x15, 0x16
  };

  static constexpr uint8_t kFailureThreshold = 5;
  static constexpr SteadyClock::duration kRegisterAccessTime = std::chrono::microseconds(600);
  static constexpr SteadyClock::duration kConversionTime = std::chrono::milliseconds(8);
  static constexpr SteadyClock::duration kRetryBackoff = std::chrono::seconds(1);

  std::shared_ptr<I2cBus> bus_;
  LifecycleState state_;
  uint8_t consecutive_failures_ = 0;
};

}  // namespace sensors::ld
