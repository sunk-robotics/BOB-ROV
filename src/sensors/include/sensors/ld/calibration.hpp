#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

#include "sensors/ld/error.hpp"
#include "sensors/ld/pressure_range.hpp"

namespace sensors::ld
{

/// What the pressure reading is referenced against.
enum class PressureMode : uint8_t
{
  /// Vented gauge. Zero at atmospheric pressure.
  vented_gauge = 0,
  /// Sealed gauge. Zero at 1.0 bar absolute.
  sealed_gauge = 1,
  /// Absolute. Zero at vacuum.
  absolute = 2
};

/// Date and pressure mode recorded when the sensor was calibrated.
struct CalibrationInfo
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  PressureMode pressure_mode;
};

/// Everything the driver reads from the sensor's calibration registers (0x12 through 0x16).
class Calibration
{
 public:
  static constexpr std::size_t kRegisterCount = 5;

  /// Decodes the five 16-bit register words, given in address order.
  /** Word 0 packs the calibration info; words 1-2 and 3-4 are the high and low halves of the
   * IEEE-754 float minimum and maximum pressure respectively. */
  [[nodiscard]] static std::expected<Calibration, std::error_code> from_register_words(
      std::span<const uint16_t, kRegisterCount> words
  ) noexcept
  {
    const uint16_t info_word = words[0];

    const auto pressure_mode_bits =
        static_cast<uint8_t>((info_word >> kPressureModeShift) & kPressureModeMask);
    if (pressure_mode_bits > static_cast<uint8_t>(PressureMode::absolute))
      return std::unexpected(make_error_code(ErrorCode::invalid_pressure_mode));

    const auto pressure_range = PressureRange::create(
        float_from_words(words[1], words[2]), float_from_words(words[3], words[4])
    );
    if (!pressure_range)
      return std::unexpected(pressure_range.error());

    return Calibration(
        CalibrationInfo{
            .year = static_cast<uint16_t>(((info_word >> kYearShift) & kYearMask) + kYearOffset),
            .month = static_cast<uint8_t>((info_word >> kMonthShift) & kMonthMask),
            .day = static_cast<uint8_t>((info_word >> kDayShift) & kDayMask),
            .pressure_mode = static_cast<PressureMode>(pressure_mode_bits)
        },
        *pressure_range
    );
  }

  [[nodiscard]] constexpr const CalibrationInfo& info() const noexcept
  {
    return info_;
  }

  [[nodiscard]] constexpr const PressureRange& pressure_range() const noexcept
  {
    return pressure_range_;
  }

 private:
  constexpr Calibration(CalibrationInfo info, PressureRange pressure_range) noexcept
      : info_(info), pressure_range_(pressure_range)
  {}

  [[nodiscard]] static constexpr float float_from_words(
      uint16_t high_word,
      uint16_t low_word
  ) noexcept
  {
    return std::bit_cast<float>((static_cast<uint32_t>(high_word) << 16) | low_word);
  }

  // Bit layout of word 0, after shifting down.
  static constexpr unsigned kYearShift = 11;
  static constexpr unsigned kMonthShift = 7;
  static constexpr unsigned kDayShift = 2;
  static constexpr unsigned kPressureModeShift = 0;

  static constexpr unsigned kYearMask = 0x1F;
  static constexpr unsigned kMonthMask = 0x0F;
  static constexpr unsigned kDayMask = 0x1F;
  static constexpr unsigned kPressureModeMask = 0x03;

  static constexpr unsigned kYearOffset = 2010;

  CalibrationInfo info_;
  PressureRange pressure_range_;
};

}  // namespace sensors::ld
