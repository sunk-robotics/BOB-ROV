#pragma once

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

namespace sensors::bno055
{

/// Error codes specific to the BNO055 IMU driver.
enum class ErrorCode : uint8_t
{
  /// The vendor library failed to initialize the device (e.g. the chip ID did not match).
  init_failed = 1,
  /// The vendor library failed to switch the device to normal power mode.
  set_power_mode_failed,
  /// The vendor library failed to set the gyroscope unit to radians per second.
  set_gyro_unit_failed,
  /// The vendor library failed to switch the device to the requested operation mode.
  set_operation_mode_failed,
  /// No I2C bus was provided.
  null_bus,
  /// The vendor library supports a single device at a time and a driver already exists.
  another_instance_exists
};

/// `std::error_category` implementation for `ErrorCode` values.
class ErrorCategory : public std::error_category
{
 public:
  [[nodiscard]] const char* name() const noexcept override
  {
    return "Bno055";
  }

  [[nodiscard]] std::string message(int error_value) const override
  {
    switch (static_cast<ErrorCode>(error_value)) {
      case ErrorCode::init_failed:
        return "Failed to init Bno055 device!";
      case ErrorCode::set_power_mode_failed:
        return "Failed to set Bno055 power mode!";
      case ErrorCode::set_gyro_unit_failed:
        return "Failed to set Bno055 gyro units to rad/s!";
      case ErrorCode::set_operation_mode_failed:
        return "Failed to set Bno055 operation mode!";
      case ErrorCode::null_bus:
        return "An I2C bus is required but a null bus was provided!";
      case ErrorCode::another_instance_exists:
        return "The Bno055 vendor library supports one device at a time and a driver already "
               "exists!";
      default:
        return "Unknown Bno055 error!";
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

}  // namespace sensors::bno055

namespace std
{
  /// Enables implicit conversion of `sensors::bno055::ErrorCode` to `std::error_code`.
  template<>
  struct is_error_code_enum<sensors::bno055::ErrorCode> : true_type {};
}  // namespace std
