#pragma once

#include <cmath>
#include <expected>
#include <system_error>

#include "sensors/ld/error.hpp"

namespace sensors::ld
{

/// The pressure span the sensor's raw output is scaled over.
/** Guaranteed finite with `maximum() > minimum()`, so conversions never divide out a zero or
 * negative span. */
class PressureRange
{
 public:
  [[nodiscard]] static std::expected<PressureRange, std::error_code> create(
      float minimum,
      float maximum
  ) noexcept
  {
    if (!(std::isfinite(minimum) && std::isfinite(maximum) && maximum > minimum))
      return std::unexpected(make_error_code(ErrorCode::invalid_pressure_range));
    return PressureRange(minimum, maximum);
  }

  [[nodiscard]] constexpr float minimum() const noexcept
  {
    return minimum_;
  }

  [[nodiscard]] constexpr float maximum() const noexcept
  {
    return maximum_;
  }

 private:
  constexpr PressureRange(float minimum, float maximum) noexcept
      : minimum_(minimum), maximum_(maximum)
  {}

  float minimum_;
  float maximum_;
};

}  // namespace sensors::ld
