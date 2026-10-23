#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>

#include "rclcpp/rclcpp.hpp"

#include "i2c_utils/i2c_bus.hpp"

/// Error codes specific to the AHT20 temperature/humidity sensor driver.
enum class Aht20ErrorCode : uint8_t {
  /// A measurement did not complete within the configured poll timeout.
  measurement_timed_out = 1,
  /// The CRC byte returned by the sensor did not match the CRC computed over the measurement data.
  crc_mismatch,
  /// `measure_timeout_ms` passed to `Aht20Driver::create()` was below the required 80ms minimum.
  invalid_measure_timeout,
  /// `measure_timeout_ms` passed to `Aht20Driver::create()` was below the required 80ms minimum.
  invalid_measure_poll_period,
};

/// `std::error_category` implementation for `Aht20ErrorCode` values.
class Aht20ErrorCategory : public std::error_category {
public:
  [[nodiscard]] const char *name() const noexcept override { return "Aht20"; }
  /// Returns a human-readable description of an `Aht20ErrorCode` value.
  /** @param ev The integer value of an `Aht20ErrorCode`.
   *  @return A descriptive message corresponding to `ev`, or a generic "Unknown Aht20 error!"
   * message if `ev` does not match any known `Aht20ErrorCode`. */
  [[nodiscard]] std::string message(int ev) const noexcept override
  {
    switch (static_cast<Aht20ErrorCode>(ev)) {
      case Aht20ErrorCode::measurement_timed_out:
        return "Measugrement did not return successfully within specified timeout period!";
      case Aht20ErrorCode::crc_mismatch: return "Recieved measurement data failed crc check!";
      case Aht20ErrorCode::invalid_measure_timeout:
        return "Measuremnet timeout values must be at least 80ms. Provided value was below this "
               "threshold.";
      case Aht20ErrorCode::invalid_measure_poll_period:
        return "Measurement wait period must be greater than 0!";
      default: return "Unknown Aht20 error!";
    }
  }
};

/// Returns the singleton `Aht20ErrorCategory` instance.
/** The instance is constructed on first use and lives for the remaining duration of the program
 * Used by `make_error_code()` to construct `std::error_code` values for `Aht20ErrorCode`. */
inline const Aht20ErrorCategory &aht20_error_category()
{
  static Aht20ErrorCategory instance;
  return instance;
}

/// Creates a `std::error_code` from an `Aht20ErrorCode`.
/** @param e The driver-specific error code to wrap.
 *  @return A `std::error_code` with value `e` and category `aht20_error_category()`. */
inline std::error_code make_error_code(Aht20ErrorCode e) noexcept
{ return {static_cast<int>(e), aht20_error_category()}; }

namespace std {
/// Registers `Aht20ErrorCode` as an error code enum.
/** Enables implicit conversion of `Aht20ErrorCode` values to `std::error_code` via
 * `make_error_code()`. */
template <> struct is_error_code_enum<Aht20ErrorCode> : true_type {};
} // namespace std

/// Raw, unconverted measurement data read from the AHT20.
/** Both fields hold the 20-bit values unpacked directly from the sensor's data registers, as
 * described in the AHT20 datasheet. Use `Aht20Driver::read()` to obtain values converted to
 * physical units. */
struct RawAht20Measurement {
  /// Raw 20-bit temperature ADC reading.
  uint32_t temperature;
  /// Raw 20-bit relative humidity ADC reading.
  uint32_t relative_humidity;
};

/// Temperature and relative humidity measurement in physical units.
/** Produced by `Aht20Driver::read()` by converting a `RawAht20Measurement`
 *  using the formulas from the AHT20 datasheet. (See `Aht20Driver::read()` documentation for
 * forumla) */
struct Aht20Measurement {
  /// Temperature in degrees Celsius.
  double temperature;
  /// Relative humidity as a fraction in the range [0.0, 1.0].
  double relative_humidity;
};

/// Driver for the AHT20 I2C temperature and humidity sensor.
/** Handles the sensor's power-on and calibration sequence, triggers measurements, polls for
 * completion, validates the CRC of the returned data, and converts raw readings into physical
 * units. Instances must be created via the `create()` factory function. */
class Aht20Driver {
public:
  Aht20Driver(const Aht20Driver &) = delete;
  Aht20Driver &operator=(const Aht20Driver &) = delete;

  /// Creates and initializes an `Aht20Driver`.
  /** Validates the provided timing parameters, waits the sensor's power-on delay, reads the
   * sensor's status byte, and, if the sensor is not yet calibrated, sends the
   * calibration/initialization command and waits for it to take effect.
   *
   *  @param bus I2C bus used to communicate with the sensor.
   *  @param measure_timeout_ms Maximum time, in milliseconds, that `read_raw()` will wait for a
   * measurement to complete. Must be at least 80ms.
   *  @param measure_poll_period_ms Interval, in milliseconds, between status polls while
   * `read_raw()` is waiting for a measurement to complete. Must be non-zero.
   *
   *  @return A `std::unique_ptr<Aht20Driver>` on success. On failure, returns
   * `Aht20ErrorCode::invalid_measure_timeout` or `Aht20ErrorCode::invalid_measure_poll_period` if
   * the timing parameters are invalid, or a `std::error_code` forwarded from the underlying
   * `I2cBus` if an I2C transfer fails. */
  [[nodiscard]] static std::expected<std::unique_ptr<Aht20Driver>, std::error_code> create(
      std::shared_ptr<I2cBus> bus,
      uint16_t measure_timeout_ms,
      uint8_t measure_poll_period_ms) noexcept
  {
    if (measure_timeout_ms < 80)
      return std::unexpected(Aht20ErrorCode::invalid_measure_timeout);
    if (measure_poll_period_ms == 0)
      return std::unexpected(Aht20ErrorCode::invalid_measure_poll_period);

    auto driver = std::unique_ptr<Aht20Driver>(
        new Aht20Driver(std::move(bus), measure_timeout_ms, measure_poll_period_ms));

    // sleep until 40ms after power up
    rclcpp::Clock clock(RCL_STEADY_TIME);
    clock.sleep_until(rclcpp::Time(40'000'000LL, RCL_STEADY_TIME));

    uint8_t status;
    auto result = driver->bus_->transfer(kI2cAddr, {&kStatusCommand, 1}, {&status, 1});
    if (!result)
      return std::unexpected(result.error());

    // if uncalibrated
    if ((status & kCalibEnabledMask) == 0) {
      auto result = driver->bus_->write(kI2cAddr, kInitCommandMessage);
      if (!result)
        return std::unexpected(result.error());

      rclcpp::sleep_for(std::chrono::milliseconds(10));
    }

    return driver;
  }

  /// Triggers a measurement and reads the raw result from the sensor.
  /** Sends the trigger-measurement command, waits the minimum 80ms measurement time required by the
   * datasheet, then repeatedly reads the sensor's status/data bytes, sleeping
   * `measure_poll_period_ms_` between reads, until the busy flag clears or the configured
   * `measure_timeout_ms_` elapses. Once the data is read, the CRC byte is validated before the
   * temperature and humidity fields are unpacked from their packed 20-bit representation.
   *
   *  @return The raw temperature and humidity ADC values on success. On failure, returns a
   * `std::error_code` forwarded from the underlying `I2cBus` if a transfer fails,
   * `Aht20ErrorCode::measurement_timed_out` if the busy flag never clears within
   * `measure_timeout_ms_`, or `Aht20ErrorCode::crc_mismatch` if the received CRC byte does not
   * match the computed CRC. */
  [[nodiscard]] std::expected<RawAht20Measurement, std::error_code> read_raw() noexcept
  {
    auto result = bus_->write(kI2cAddr, kTriggerMeasurementMessage);
    if (!result)
      return std::unexpected(result.error());

    rclcpp::sleep_for(std::chrono::milliseconds(80));

    std::array<uint8_t, 7> measurement_data;
    uint16_t elapsed_ms = 80;
    bool timed_out = true;
    while (true) {
      auto result = bus_->read(kI2cAddr, measurement_data);
      if (!result)
        return std::unexpected(result.error());

      // if measurement completed
      if ((measurement_data[0] & kMeasurementBusyMask) == 0) {
        timed_out = false;
        break;
      }

      rclcpp::sleep_for(std::chrono::milliseconds(measure_poll_period_ms_));
      elapsed_ms += measure_poll_period_ms_;
      if (elapsed_ms >= measure_timeout_ms_)
        break;
    }

    if (timed_out) {
      return std::unexpected(Aht20ErrorCode::measurement_timed_out);
    }

    // loop over all bytes before the crc byte
    uint8_t crc = kCrcInitVal;
    for (uint8_t byte_idx = 0; byte_idx < measurement_data.size() - 1; ++byte_idx) {
      crc ^= measurement_data[byte_idx];
      for (uint8_t bit_idx = 0; bit_idx < 8; ++bit_idx) {
        if (crc & 0x80)
          crc = (crc << 1) ^ kCrcPolynomial;
        else
          crc <<= 1;
      }
    }

    if (crc != measurement_data[6])
      return std::unexpected(make_error_code(Aht20ErrorCode::crc_mismatch));

    // promote to uint32_t in one go to avoid a billion static casts
    const uint32_t b1 = measurement_data[1], b2 = measurement_data[2], b3 = measurement_data[3],
                   b4 = measurement_data[4], b5 = measurement_data[5];

    // Temperature and humidity are both 20 bits so we pack it into two uint32_ts
    return RawAht20Measurement{
        .temperature = (b1 << 12) | (b2 << 4) | (b3 >> 4),
        .relative_humidity = ((b3 & 0x0F) << 16) | (b4 << 8) | b5};
  }

  /// Triggers a measurement and reads the result in physical units.
  /** Calls `read_raw()` and converts the resulting raw ADC values to degrees Celsius and a
   * fractional relative humidity using the conversion formulas from the AHT20 datasheet.
   *
   * For temperature:
   * \f[
   *    T[^\circ\mathrm{C}] = \left(\frac{S_{\mathrm{RH}}}{2^{20}}\right) \cdot 200 - 50
   * \f]
   * and for relative humidity:
   * \f[
   *    RH[\%] = \left(\frac{S_{\mathrm{RH}}}{2^{20}}\right) \cdot 100\%
   * \f]
   *
   *  @return The converted measurement on success, or the `std::error_code` forwarded unchanged
   * from `read_raw()` on failure. */
  [[nodiscard]] std::expected<Aht20Measurement, std::error_code> read() noexcept
  {
    auto result = read_raw();
    if (!result)
      return std::unexpected(result.error());

    return Aht20Measurement{
        // see datasheet
        .temperature = result.value().temperature / 1048576.0 * 200.0 - 50.0,
        .relative_humidity = result.value().relative_humidity / 1048576.0};
  }

private:
  /// Constructs a driver with the given bus and timing configuration.
  /** Only intended to be called from `create()`, which performs the power-on and calibration
   * sequence required before the driver is ready for use. */
  explicit Aht20Driver(
      std::shared_ptr<I2cBus> bus,
      uint16_t measure_timeout_ms,
      uint8_t measure_poll_period_ms)
      : bus_(std::move(bus)), measure_timeout_ms_(measure_timeout_ms),
        measure_poll_period_ms_(measure_poll_period_ms)
  {
  }

  static constexpr uint8_t kI2cAddr = 0x38;
  static constexpr uint8_t kStatusCommand = 0x71;
  static constexpr uint8_t kCalibEnabledMask = (0x1 << 3);
  static constexpr uint8_t kMeasurementBusyMask = (0x01 << 7);
  static constexpr std::array<uint8_t, 3> kInitCommandMessage{0xBE, 0x08, 0x00};
  static constexpr std::array<uint8_t, 3> kTriggerMeasurementMessage{0xAC, 0x33, 0x00};
  static constexpr uint8_t kCrcInitVal = 0xFF;
  static constexpr uint8_t kCrcPolynomial = 0x31;

  std::shared_ptr<I2cBus> bus_;
  /// Maximum time, in milliseconds, that `read_raw()` will wait for a measurement to complete.
  uint16_t measure_timeout_ms_;
  /// Maximum time, in milliseconds, that `read_raw()` will wait for a measurement to complete after
  /// initial delay.
  uint8_t measure_poll_period_ms_;
};
