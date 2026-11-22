#pragma once

#include <expected>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>

#include <i2c/i2c_bus.hpp>
#include "sensors/ld/driver.hpp"
#include "sensors/ld/error.hpp"
#include "sensors/ld/measurement.hpp"

namespace sensors::ld
{

/// Blocking convenience layer over the non-blocking `Driver`.
/** Each `read*()` call loads the calibration if needed, triggers a measurement, and sleeps until
 * the result arrives or an error occurs. It sleeps with `std::this_thread`, which unlike
 * `rclcpp::sleep_for` is not interrupted by ROS shutdown; ROS nodes should prefer driving `Driver`
 * from a timer using `Driver::next_action_time()`. */
class BlockingReader
{
 public:
  [[nodiscard]] static std::expected<BlockingReader, std::error_code> create(
      std::shared_ptr<I2cBus> bus
  ) noexcept
  {
    auto driver = Driver::create(std::move(bus));
    if (!driver)
      return std::unexpected(driver.error());
    return BlockingReader(std::move(*driver));
  }

  [[nodiscard]] std::expected<RawMeasurement, std::error_code> read_raw() noexcept
  {
    if (auto ready = wait_until_ready(); !ready)
      return std::unexpected(ready.error());

    if (auto triggered = driver_->trigger_measurement(); !triggered)
      return std::unexpected(triggered.error());

    // Once the conversion time has elapsed try_read_raw() either returns data or an error,
    // so this loop always terminates.
    while (true) {
      std::this_thread::sleep_until(driver_->next_action_time());
      auto result = driver_->try_read_raw();
      if (!result)
        return std::unexpected(result.error());
      if (*result)
        return **result;
    }
  }

  [[nodiscard]] std::expected<Measurement, std::error_code> read() noexcept
  {
    const auto raw = read_raw();
    if (!raw)
      return std::unexpected(raw.error());

    const auto calibration = driver_->calibration();
    if (!calibration)
      return std::unexpected(make_error_code(ErrorCode::not_ready));

    return Measurement::from_raw(*raw, calibration->pressure_range());
  }

 private:
  explicit BlockingReader(std::unique_ptr<Driver> driver) noexcept : driver_(std::move(driver)) {}

  /// Each step either advances calibration loading or returns an error, so this terminates.
  [[nodiscard]] std::expected<void, std::error_code> wait_until_ready() noexcept
  {
    while (!driver_->is_ready()) {
      std::this_thread::sleep_until(driver_->next_action_time());
      const auto progress = driver_->recover_step();
      if (!progress)
        return std::unexpected(progress.error());
    }
    return {};
  }

  std::unique_ptr<Driver> driver_;
};

}  // namespace sensors::ld
