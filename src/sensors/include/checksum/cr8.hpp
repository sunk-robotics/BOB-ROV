#pragma once

#include <cstdint>
#include <span>

/// Bitwise (MSB-first) CRC-8 calculator, parameterized by polynomial and initial value.
class Crc8
{
 public:
  constexpr Crc8(uint8_t polynomial, uint8_t initial_value) noexcept
      : polynomial_(polynomial), initial_value_(initial_value)
  {}

  [[nodiscard]] constexpr uint8_t calculate(std::span<const uint8_t> data) const noexcept
  {
    uint8_t checksum = initial_value_;
    for (const uint8_t byte : data) {
      checksum ^= byte;
      for (uint8_t bit_index = 0; bit_index < 8; ++bit_index) {
        const bool most_significant_bit_set = (checksum & 0x80) != 0;
        checksum = static_cast<uint8_t>(checksum << 1);
        if (most_significant_bit_set)
          checksum ^= polynomial_;
      }
    }
    return checksum;
  }

 private:
  const uint8_t polynomial_;
  const uint8_t initial_value_;
};
