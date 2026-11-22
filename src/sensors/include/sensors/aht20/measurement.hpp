#pragma once

#include <cstdint>
#include <span>


namespace sensors::aht20 
{

/// Raw, unconverted measurement data read from the AHT20.
/** Both values are 20-bit ADC readings. The fields are private and the only way to construct an
 * instance is `from_data()`, so the 20-bit range is guaranteed by construction. */
class RawMeasurement
{
 public:
  /// Unpacks the five data bytes that follow the status byte in a sensor response.
  /** Per the datasheet the layout is: 20 bits of humidity followed by 20 bits of temperature. */
  [[nodiscard]] static constexpr RawMeasurement from_data(
      std::span<const uint8_t, 5> data
  ) noexcept
  {
    const uint32_t humidity_high_byte = data[0];
    const uint32_t humidity_low_byte = data[1];
    const uint32_t shared_byte = data[2];  // humidity[3:0] in upper nibble, temperature[19:16] lower
    const uint32_t temperature_middle_byte = data[3];
    const uint32_t temperature_low_byte = data[4];

    return RawMeasurement(
        (shared_byte & 0x0F) << 16 | temperature_middle_byte << 8 | temperature_low_byte,
        humidity_high_byte << 12 | humidity_low_byte << 4 | shared_byte >> 4
    );
  }

  /// Raw 20-bit temperature ADC reading.
  [[nodiscard]] constexpr uint32_t temperature() const noexcept
  {
    return temperature_;
  }

  /// Raw 20-bit relative humidity ADC reading.
  [[nodiscard]] constexpr uint32_t relative_humidity() const noexcept
  {
    return relative_humidity_;
  }

 private:
  constexpr RawMeasurement(uint32_t temperature, uint32_t relative_humidity) noexcept
      : temperature_(temperature), relative_humidity_(relative_humidity)
  {}

  uint32_t temperature_;
  uint32_t relative_humidity_;
};

/// Temperature and relative humidity measurement in physical units.
struct Measurement
{
  /// Temperature in degrees Celsius.
  double temperature_celsius;
  /// Relative humidity as a fraction in the range [0.0, 1.0].
  double relative_humidity_fraction;

  /// Converts raw ADC values to physical units using the AHT20 datasheet formulas.
  /**
   * \f[
   *    T[^\circ\mathrm{C}] = \frac{S_{\mathrm{T}}}{2^{20}} \cdot 200 - 50
   * \f]
   * \f[
   *    RH = \frac{S_{\mathrm{RH}}}{2^{20}}
   * \f]
   * where \f$RH\f$ is a fraction (multiply by 100 for percent). */
  [[nodiscard]] static constexpr Measurement from_raw(Measurement raw) noexcept
  {
    constexpr double kFullScale = 1048576.0;  // 2^20
    return Measurement{
        .temperature_celsius = raw.temperature_celsius / kFullScale * 200.0 - 50.0,
        .relative_humidity_fraction = raw.relative_humidity_fraction / kFullScale
    };
  }
};

}
