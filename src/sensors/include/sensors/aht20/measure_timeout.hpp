#pragma once
 
#include <chrono>
#include <expected>
#include <system_error>
 
#include "sensors/aht20/error.hpp"
 
namespace sensors::aht20 
{

/// A measurement timeout that is guaranteed to be at least the sensor's conversion time.
class MeasureTimeout
{
 public:
  using Duration = std::chrono::steady_clock::duration;
 
  /// The datasheet requires at least this long between triggering and reading a result.
  static constexpr Duration kMinimumConversionTime = std::chrono::milliseconds(80);
 
  [[nodiscard]] static std::expected<MeasureTimeout, std::error_code> create(
      Duration duration
  ) noexcept
  {
    if (duration < kMinimumConversionTime)
      return std::unexpected(make_error_code(aht20::ErrorCode::invalid_measure_timeout));
    return MeasureTimeout(duration);
  }
 
  [[nodiscard]] constexpr Duration duration() const noexcept
  {
    return duration_;
  }
 
 private:
  explicit constexpr MeasureTimeout(Duration duration) noexcept : duration_(duration) {}
 
  Duration duration_;
};
 
}
