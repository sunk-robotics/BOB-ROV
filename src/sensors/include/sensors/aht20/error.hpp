#pragma once

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>


namespace sensors::aht20 
{

/// Error codes specific to the AHT20 temperature/humidity sensor driver.
enum class ErrorCode : uint8_t
{
  /// A measurement did not complete within the configured measure timeout.
  measurement_timed_out = 1,
  /// The CRC byte returned by the sensor did not match the CRC computed over the data.
  crc_mismatch,
  /// The measure timeout was below the required 80ms minimum.
  invalid_measure_timeout,
  /// The poll period was zero or negative.
  invalid_poll_period,
  /// The sensor did not report itself calibrated after the initialization command.
  initialization_failed,
  /// No I2C bus was provided.
  null_bus,
  /// The driver is still resetting/initializing, so the requested operation is not available.
  not_ready,
  /// A measurement was requested while a previous one is still in flight.
  measurement_already_in_progress,
  /// A result was requested but no measurement has been triggered.
  no_measurement_in_progress
};

/// `std::error_category` implementation for `aht20::ErrorCode` values.
class ErrorCategory : public std::error_category
{
 public:
  [[nodiscard]] const char* name() const noexcept override
  {
    return "Aht20";
  }

  /// Returns a human-readable description of an `aht20::ErrorCode` value.
  /** @param error_value The integer value of an `aht20::ErrorCode`.
   *  @return A descriptive message, or "Unknown Aht20 error!" for unrecognized values. */
  [[nodiscard]] std::string message(int error_value) const override
  {
    switch (static_cast<aht20::ErrorCode>(error_value)) {
      case aht20::ErrorCode::measurement_timed_out:
        return "Measurement did not complete within the specified timeout period!";
      case aht20::ErrorCode::crc_mismatch:
        return "Received measurement data failed CRC check!";
      case aht20::ErrorCode::invalid_measure_timeout:
        return "Measurement timeout must be at least 80ms. Provided value was below this "
               "threshold.";
      case aht20::ErrorCode::invalid_poll_period:
        return "Poll period must be greater than 0!";
      case aht20::ErrorCode::initialization_failed:
        return "Sensor did not report calibration enabled after initialization command!";
      case aht20::ErrorCode::null_bus:
        return "An I2C bus is required but a null bus was provided!";
      case aht20::ErrorCode::not_ready:
        return "Sensor is still resetting or initializing!";
      case aht20::ErrorCode::measurement_already_in_progress:
        return "A measurement is already in progress!";
      case aht20::ErrorCode::no_measurement_in_progress:
        return "No measurement has been triggered!";
      default:
        return "Unknown Aht20 error!";
    }
  }
};

/// Returns the singleton `Aht20ErrorCategory` instance.
inline const ErrorCategory& aht20_error_category() noexcept
{
  static const ErrorCategory instance;
  return instance;
}

/// Creates a `std::error_code` from an `aht20::ErrorCode`.
inline std::error_code make_error_code(aht20::ErrorCode error_code) noexcept
{
  return {static_cast<int>(error_code), aht20_error_category()};
}

}

namespace std
{
  /// Enables implicit conversion of `aht20::ErrorCode` to `std::error_code`.
  template<>
  struct is_error_code_enum<aht20::ErrorCode> : true_type {};
}  // namespace std


