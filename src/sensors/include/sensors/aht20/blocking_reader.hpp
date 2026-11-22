#pragma once

#include <chrono>
#include <expected>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>

#include "sensors/aht20/driver.hpp"
#include "sensors/aht20/error.hpp"
#include "sensors/aht20/measure_timeout.hpp"
#include "sensors/aht20/measurement.hpp"

namespace sensors::aht20 
{

/// A strictly positive interval between polls.
class PollPeriod
{
 public:
  using Duration = std::chrono::steady_clock::duration;

  [[nodiscard]] static std::expected<PollPeriod, std::error_code> create(Duration duration) noexcept
  {
    if (duration <= Duration::zero())
      return std::unexpected(make_error_code(Aht20ErrorCode::invalid_poll_period));
    return PollPeriod(duration);
  }

  [[nodiscard]] constexpr Duration duration() const noexcept
  {
    return duration_;
  }

 private:
  explicit constexpr PollPeriod(Duration duration) noexcept : duration_(duration) {}

  Duration duration_;
};

/// Blocking convenience layer over the non-blocking `aht20::Driver`.
/** Each `read*()` call brings the sensor up if needed, triggers a measurement, and sleeps until
 * the result arrives or an error occurs. Intended for simple, dedicated-thread use. */
class BlockingReader
{
 public:
  [[nodiscard]] static std::expected<BlockingReader, std::error_code> create(
      std::shared_ptr<I2cBus> bus,
      MeasureTimeout measure_timeout,
      PollPeriod poll_period
  ) noexcept
  {
    auto driver = aht20::Driver::create(std::move(bus), measure_timeout);
    if (!driver)
      return std::unexpected(driver.error());
    return BlockingReader(std::move(*driver), poll_period);
  }

  [[nodiscard]] std::expected<aht20::RawMeasurement, std::error_code> read_raw() noexcept
  {
    if (auto ready = wait_until_ready(); !ready)
      return std::unexpected(ready.error());

    if (auto triggered = driver_->trigger_measurement(); !triggered)
      return std::unexpected(triggered.error());

    std::this_thread::sleep_for(MeasureTimeout::kMinimumConversionTime);

    // try_read_raw() ends the measurement with an error once the measure timeout elapses,
    // so this loop always terminates.
    while (true) {
      auto result = driver_->try_read_raw();
      if (!result)
        return std::unexpected(result.error());
      if (*result)
        return **result;
      std::this_thread::sleep_for(poll_period_.duration());
    }
  }

  [[nodiscard]] std::expected<aht20::Measurement, std::error_code> read() noexcept
  {
    return read_raw().transform(aht20::Measurement::from_raw);
  }

 private:
  BlockingReader(std::unique_ptr<aht20::Driver> driver, PollPeriod poll_period) noexcept
      : driver_(std::move(driver)), poll_period_(poll_period)
  {}

  /// Each step either advances the sequence or returns an error, so this terminates.
  [[nodiscard]] std::expected<void, std::error_code> wait_until_ready() noexcept
  {
    while (!driver_->is_ready()) {
      const auto progress = driver_->recover_step();
      if (!progress)
        return std::unexpected(progress.error());
      if (*progress == aht20::Driver::RecoveryProgress::recovered)
        break;
      std::this_thread::sleep_for(poll_period_.duration());
    }
    return {};
  }

  std::unique_ptr<aht20::Driver> driver_;
  PollPeriod poll_period_;
};

}
