#pragma once

#include <expected>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

#include "rclcpp/rclcpp.hpp"

#include "i2c_utils/i2c_bus.hpp"

extern "C" {
#include "bno055.h"
}

enum class Bno055ErrorCode : uint8_t {
  init_failed = 1,
  set_power_mode_failed,
  set_op_mode_failed,
  set_gyro_units_failed
};

class Bno055ErrorCategory : public std::error_category {
public:
  [[nodiscard]] const char *name() const noexcept override { return "Bno055"; }
  [[nodiscard]] std::string message(int ev) const noexcept override
  {
    switch (static_cast<Bno055ErrorCode>(ev)) {
      case Bno055ErrorCode::init_failed: return "Failed to init Bno055 device!";
      case Bno055ErrorCode::set_power_mode_failed: return "Failed to set Bno055 power mode!";
      case Bno055ErrorCode::set_op_mode_failed: return "Failed to set Bno055 operation mode!";
      case Bno055ErrorCode::set_gyro_units_failed:
        return "Failed to set Bno055 gyro units to rad/s!";
      default: return "Unknown Bno055 error!";
    }
  }
};

inline const Bno055ErrorCategory &bno055_error_category()
{
  static Bno055ErrorCategory instance;
  return instance;
}

inline std::error_code make_error_code(Bno055ErrorCode e) noexcept
{ return {static_cast<int>(e), bno055_error_category()}; }

namespace std {
template <> struct is_error_code_enum<Bno055ErrorCode> : true_type {};
} // namespace std

class Bno055Driver {
public:
  Bno055Driver(const Bno055Driver &) = delete;
  Bno055Driver &operator=(const Bno055Driver &) = delete;

  [[nodiscard]] static std::expected<std::unique_ptr<Bno055Driver>, std::error_code> create(
      std::shared_ptr<I2cBus> bus,
      bool use_alternate_addr = false,
      u8 op_mode = BNO055_OPERATION_MODE_NDOF) noexcept
  {
    auto driver = std::unique_ptr<Bno055Driver>(new Bno055Driver(std::move(bus)));

    // hook up library functions
    driver->dev_.dev_addr = use_alternate_addr ? BNO055_I2C_ADDR2 : BNO055_I2C_ADDR1;
    driver->dev_.bus_write = &Bno055Driver::bus_write_callback;
    driver->dev_.bus_read = &Bno055Driver::bus_read_callback;
    driver->dev_.delay_msec = &Bno055Driver::delay_millis_callback;
    driver->dev_.userdata = driver->bus_.get();

    if (s8 rc = bno055_init(&driver->dev_); rc == BNO055_ERROR) {
      return std::unexpected(Bno055ErrorCode::init_failed);
    }
    // Normal mode for running sensors at optimal refresh rates
    if (s8 rc = bno055_set_power_mode(BNO055_POWER_MODE_NORMAL); rc == BNO055_ERROR) {
      return std::unexpected(Bno055ErrorCode::set_power_mode_failed);
    }
    if (s8 rc = bno055_set_operation_mode(op_mode); rc == BNO055_ERROR) {
      return std::unexpected(Bno055ErrorCode::set_op_mode_failed);
    }
    // radians because we're not insane
    if (s8 rc = bno055_set_gyro_unit(BNO055_GYRO_UNIT_RPS); rc == BNO055_ERROR) {
      return std::unexpected(Bno055ErrorCode::set_gyro_units_failed);
    }

    return driver;
  }

private:
  explicit Bno055Driver(std::shared_ptr<I2cBus> bus) : bus_(std::move(bus)) {}

  std::shared_ptr<I2cBus> bus_;
  bno055_t dev_{};

  static s8 bus_write_callback(u8 dev_addr, u8 reg_addr, u8 *data, u8 len, void *userdata) noexcept
  {
    // library appears to only write length of one; enables avoiding large buffer allocation.
    // guarded against just in case.
    if (len != 1)
      return BNO055_ERROR;
    std::array<uint8_t, 2> buf = {reg_addr, data[0]};

    auto *bus = static_cast<I2cBus *>(userdata);

    auto result = bus->write(dev_addr, buf);
    return result ? BNO055_SUCCESS : BNO055_ERROR;
  }

  static s8 bus_read_callback(u8 dev_addr, u8 reg_addr, u8 *data, u8 len, void *userdata) noexcept
  {
    auto *bus = static_cast<I2cBus *>(userdata);

    // write register and then read in data
    auto result = bus->transfer(dev_addr, {&reg_addr, 1}, {data, len});
    return result ? BNO055_SUCCESS : BNO055_ERROR;
  }

  static void delay_millis_callback(u32 msec) noexcept
  { rclcpp::sleep_for(std::chrono::milliseconds(msec)); }
};
