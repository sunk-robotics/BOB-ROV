#pragma once

#include <cstdint>
#include <span>

#include "sensors/ld/pressure_range.hpp"

namespace sensors::ld
{

/// Raw, unconverted measurement data read from the sensor.
/** Every 16-bit value is meaningful, so there is no invariant to protect and the fields are
 * public. */
struct RawMeasurement
{
  uint16_t pressure;
  uint16_t temperature;

  /// Unpacks the four data bytes that follow the status byte in a sensor response.
  [[nodiscard]] static constexpr RawMeasurement from_data(std::span<const uint8_t, 4> data) noexcept
  {
    return RawMeasurement{
        .pressure = static_cast<uint16_t>((data[0] << 8) | data[1]),
        .temperature = static_cast<uint16_t>((data[2] << 8) | data[3])
    };
  }
};

/// Pressure and temperature in physical units.
struct Measurement
{
  /// Pressure in bar.
  double pressure_bar;
  /// Temperature in degrees Celsius.
  double temperature_celsius;

  /// Converts raw values using the formulas from the sensor's communication protocol datasheet:
  /// https://web.archive.org/web/20251231045705/https://www.keller-druck2.ch/swupdate/InstallerD-LineAddressManager/manual/Communication_Protocol_4LD-9LD_en.pdf
  /**
   * \f[
   *    P = \frac{P_{\mathrm{raw}} - 16384}{32768} \cdot (P_{\max} - P_{\min}) + P_{\min}
   * \f]
   * \f[
   *    T[^\circ\mathrm{C}] = \left(\left\lfloor\frac{T_{\mathrm{raw}}}{16}\right\rfloor - 24\right)
   *                          \cdot 0.05 - 50
   * \f] */
  [[nodiscard]] static constexpr Measurement from_raw(
      RawMeasurement raw,
      const PressureRange& pressure_range
  ) noexcept
  {
    // Do the math at double precision.
    const double minimum = pressure_range.minimum();
    const double maximum = pressure_range.maximum();

    return Measurement{
        .pressure_bar =
            (static_cast<double>(raw.pressure) - 16'384.0) / 32'768.0 * (maximum - minimum)
            + minimum,
        .temperature_celsius = (static_cast<int>(raw.temperature >> 4) - 24) * 0.05 - 50.0
    };
  }
};

}  // namespace sensors::ld
