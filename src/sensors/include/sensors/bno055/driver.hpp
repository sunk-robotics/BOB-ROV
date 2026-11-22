#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <system_error>
#include <thread>
#include <utility>

#include "sensors/bno055/configuration.hpp"
#include "sensors/bno055/error.hpp"
#include <i2c/i2c_bus.hpp>

extern "C"
{
#include "bno055.h"
}

namespace sensors::bno055
{

/// RAII wrapper that connects the Bosch BNO055 vendor library to an `I2cBus`.
/** The vendor library is blocking by design: it calls back into `delay_milliseconds_callback()`
 * during initialization and mode switches, so `create()` blocks for those delays. There is
 * therefore no non-blocking state machine here, unlike the AHT20 and LD drivers.
 *
 * Once `create()` succeeds the sensor is configured and the vendor library's own read functions
 * (`bno055_read_*`) can be called. The library keeps a single global pointer to the active device,
 * so only one `Driver` may exist at a time (enforced by `create()`), the library must not be
 * called from several threads at once, and it must not be called after the `Driver` is destroyed. */
class Driver
{
 public:
  Driver(const Driver&) = delete;
  Driver& operator=(const Driver&) = delete;

  /// Creates and configures a driver.
  /** Initializes the device, sets normal power mode and radians-per-second gyroscope units,
   * then enters the requested operation mode.
   *
   *  @return The driver, or the first `ErrorCode` that occurred. */
  [[nodiscard]] static std::expected<std::unique_ptr<Driver>, std::error_code> create(
      std::shared_ptr<I2cBus> bus,
      Configuration configuration = Configuration{}
  ) noexcept
  {
    if (!bus)
      return std::unexpected(make_error_code(ErrorCode::null_bus));

    if (instance_exists_.exchange(true))
      return std::unexpected(make_error_code(ErrorCode::another_instance_exists));

    // From here on the instance flag is owned by `driver`, and released by its destructor.
    auto driver = std::unique_ptr<Driver>(new Driver(std::move(bus), configuration.i2c_address));

    if (auto initialized = driver->initialize(configuration.operation_mode); !initialized)
      return std::unexpected(initialized.error());

    return driver;
  }

  ~Driver()
  {
    instance_exists_.store(false);
  }

 private:
  Driver(std::shared_ptr<I2cBus> bus, I2cAddress i2c_address) : bus_(std::move(bus))
  {
    device_.dev_addr = static_cast<u8>(i2c_address);
    device_.bus_write = &Driver::bus_write_callback;
    device_.bus_read = &Driver::bus_read_callback;
    device_.delay_msec = &Driver::delay_milliseconds_callback;
    device_.userdata = bus_.get();
  }

  [[nodiscard]] std::expected<void, std::error_code> initialize(
      OperationMode operation_mode
  ) noexcept
  {
    if (bno055_init(&device_) != BNO055_SUCCESS)
      return std::unexpected(make_error_code(ErrorCode::init_failed));

    // Normal mode for running sensors at optimal refresh rates
    if (bno055_set_power_mode(BNO055_POWER_MODE_NORMAL) != BNO055_SUCCESS)
      return std::unexpected(make_error_code(ErrorCode::set_power_mode_failed));

    // The sensor is in configuration mode after init, and configuration registers can only be
    // written in that mode, so units must be set before entering the operation mode.
    if (bno055_set_gyro_unit(BNO055_GYRO_UNIT_RPS) != BNO055_SUCCESS)
      return std::unexpected(make_error_code(ErrorCode::set_gyro_unit_failed));

    if (bno055_set_operation_mode(static_cast<u8>(operation_mode)) != BNO055_SUCCESS)
      return std::unexpected(make_error_code(ErrorCode::set_operation_mode_failed));

    return {};
  }

  static constexpr u8 kMaximumWriteLength = 1;

  static s8 bus_write_callback(
      u8 device_address,
      u8 register_address,
      u8* data,
      u8 length,
      void* userdata
  ) noexcept
  {
    if (length > kMaximumWriteLength)
      return BNO055_ERROR;

    // The register address and payload go out in a single transaction.
    std::array<uint8_t, 1 + kMaximumWriteLength> message{};
    message[0] = register_address;
    std::copy_n(data, length, message.begin() + 1);

    auto* bus = static_cast<I2cBus*>(userdata);
    const auto result =
        bus->write(device_address, std::span<const uint8_t>{message.data(), 1u + length});
    return result ? BNO055_SUCCESS : BNO055_ERROR;
  }

  static s8 bus_read_callback(
      u8 device_address,
      u8 register_address,
      u8* data,
      u8 length,
      void* userdata
  ) noexcept
  {
    auto* bus = static_cast<I2cBus*>(userdata);

    // Write the register address, then read the data back.
    const std::array<uint8_t, 1> request{register_address};
    const auto result =
        bus->transfer(device_address, request, std::span<uint8_t>{data, length});
    return result ? BNO055_SUCCESS : BNO055_ERROR;
  }

  static void delay_milliseconds_callback(u32 milliseconds) noexcept
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
  }

  /// The vendor library keeps a global pointer to the active device, so only one can exist.
  static inline std::atomic<bool> instance_exists_{false};

  std::shared_ptr<I2cBus> bus_;
  bno055_t device_{};
};

}  // namespace sensors::bno055
