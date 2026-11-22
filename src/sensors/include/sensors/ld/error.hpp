#pragma once

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

namespace sensors::ld
{

/// Error codes specific to the Keller LD pressure/temperature sensor driver.
enum class ErrorCode : uint8_t
{
  /// The status byte of a response had its memory error flag set.
  memory_error = 1,
  /// The sensor was still busy after the wait period specified by the datasheet.
  response_timed_out,
  /// The pressure range stored on the sensor was not finite or its maximum was not above its
  /// minimum.
  invalid_pressure_range,
  /// The pressure mode stored on the sensor used the reserved value.
  invalid_pressure_mode,
  /// No I2C bus was provided.
  null_bus,
  /// The driver is still loading calibration data, so the requested operation is not available.
  not_ready,
  /// A measurement was requested while a previous one is still in flight.
  measurement_already_in_progress,
  /// A result was requested but no measurement has been triggered.
  no_measurement_in_progress
};

/// `std::error_category` implementation for `ErrorCode` values.
class ErrorCategory : public std::error_category
{
 public:
  [[nodiscard]] const char* name() const noexcept override
  {
    return "Ld";
  }

  [[nodiscard]] std::string message(int error_value) const override
  {
    switch (static_cast<ErrorCode>(error_value)) {
      case ErrorCode::memory_error:
        return "Response status byte memory error flag was set!";
      case ErrorCode::response_timed_out:
        return "Sensor still busy after the wait period specified by the datasheet!";
      case ErrorCode::invalid_pressure_range:
        return "Pressure range stored on the sensor is invalid!";
      case ErrorCode::invalid_pressure_mode:
        return "Pressure mode stored on the sensor is the reserved value!";
      case ErrorCode::null_bus:
        return "An I2C bus is required but a null bus was provided!";
      case ErrorCode::not_ready:
        return "Sensor is still loading calibration data!";
      case ErrorCode::measurement_already_in_progress:
        return "A measurement is already in progress!";
      case ErrorCode::no_measurement_in_progress:
        return "No measurement has been triggered!";
      default:
        return "Unknown LD error!";
    }
  }
};

/// Returns the singleton `ErrorCategory` instance.
inline const ErrorCategory& error_category() noexcept
{
  static const ErrorCategory instance;
  return instance;
}

/// Creates a `std::error_code` from an `ErrorCode`. Found by ADL.
inline std::error_code make_error_code(ErrorCode error_code) noexcept
{
  return {static_cast<int>(error_code), error_category()};
}

}  // namespace sensors::ld

namespace std
{
  /// Enables implicit conversion of `sensors::ld::ErrorCode` to `std::error_code`.
  template<>
  struct is_error_code_enum<sensors::ld::ErrorCode> : true_type {};
}  // namespace std
