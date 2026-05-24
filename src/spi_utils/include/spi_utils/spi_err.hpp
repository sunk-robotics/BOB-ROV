#pragma once

#include <cstdint>
#include <string>
#include <system_error>

enum class SpiErrorCode : uint8_t {
  cs_in_use = 1,
  invalid_bus,
  invalid_cs,
  missmatched_transfer_bus_size
};

class SpiErrorCategory : public std::error_category {
public:
  const char *name() const noexcept override { return "Spi"; }
  std::string message(int ev) const noexcept override
  {
    switch (static_cast<SpiErrorCode>(ev)) {
    case SpiErrorCode::cs_in_use: return "Cs already in use for selected bus!";
    case SpiErrorCode::invalid_bus: return "Provided Bus is invalid!";
    case SpiErrorCode::invalid_cs: return "Provided Cs index is invalid!";
    case SpiErrorCode::missmatched_transfer_bus_size:
      return "Provided read and write buffers are different sizes!";
    default: return "Unknown Spi error";
    }
  }
};

inline const SpiErrorCategory &i2c_error_category() noexcept
{
  static SpiErrorCategory instance;
  return instance;
}

inline std::error_code make_error_code(SpiErrorCode e) noexcept
{ return {static_cast<int>(e), i2c_error_category()}; }

namespace std {
template <> struct is_error_code_enum<SpiErrorCode> : true_type {};
} // namespace std
