#include <bit>
#include <cstdint>
#include <expected>
#include <memory>
#include <stdfloat>
#include <string>
#include <system_error>
#include <type_traits>

#include "rclcpp/rclcpp.hpp"

#include "i2c_utils/i2c_bus.hpp"

enum class LdErrorCode : uint8_t { memory_error = 1, response_timed_out };

class LdErrorCategory : public std::error_category {
public:
  [[nodiscard]] const char *name() const noexcept override { return "LD"; }
  [[nodiscard]] std::string message(int ev) const noexcept override
  {
    switch (static_cast<LdErrorCode>(ev)) {
      case LdErrorCode::memory_error: return "Measurement status byte memory error flag = 1!";
      case LdErrorCode::response_timed_out:
        return "Sensor still busy after wait peroid specified by datasheet!";
      default: return "Unknown LD error!";
    }
  }
};

inline const LdErrorCategory &ld_error_category()
{
  static LdErrorCategory instance;
  return instance;
}

inline std::error_code make_error_code(LdErrorCode e)
{ return {static_cast<int>(e), ld_error_category()}; }

namespace std {
template <> struct is_error_code_enum<LdErrorCode> : true_type {};
} // namespace std

class LdDriver {
public:
  LdDriver(const LdDriver &) = delete;
  LdDriver &operator=(const LdDriver &) = delete;

  [[nodiscard]] static std::expected<std::unique_ptr<LdDriver>, std::error_code>
  create(std::shared_ptr<I2cBus> bus) noexcept
  {
    auto driver = std::unique_ptr<LdDriver>(new LdDriver(bus));

    auto scaling1_res = driver->read_register(kScaling1Addr);
    if (!scaling1_res)
      return std::unexpected(scaling1_res.error());

    auto scaling2_res = driver->read_register(kScaling2Addr);
    if (!scaling2_res)
      return std::unexpected(scaling2_res.error());

    auto scaling3_res = driver->read_register(kScaling3Addr);
    if (!scaling3_res)
      return std::unexpected(scaling3_res.error());

    auto scaling4_res = driver->read_register(kScaling4Addr);
    if (!scaling4_res)
      return std::unexpected(scaling4_res.error());

    uint32_t pressure_min_bits =
        (static_cast<uint32_t>(scaling1_res.value()) << 16) | scaling2_res.value();
    driver->pressure_min_ = std::bit_cast<float>(pressure_min_bits);

    uint32_t pressure_max_bits =
        (static_cast<uint32_t>(scaling3_res.value()) << 16) | scaling4_res.value();
    driver->pressure_max_ = std::bit_cast<float>(pressure_max_bits);

    return driver;
  }

  enum PMode : uint8_t {
    PR = 0, // Vented Gauge. Zero at atmospheric pressure
    PA = 1, // Sealed Gauge. Zero at 1,0 bar abs.
    PAA = 2 // Absolute. Zero at vacuuume
  };

  struct CalibInfo {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    PMode p_mode;
  };

  [[nodiscard]] std::expected<CalibInfo, std::error_code> get_calib_info() noexcept
  {
    auto scaling0_res = read_register(kScaling0Addr);
    if (!scaling0_res)
      return std::unexpected(scaling0_res.error());

    uint16_t value = scaling0_res.value();

    // compiler sees adding 2010 to another uint16_t,
    // no way to prove it won't overflow so we need explicit cast for .year
    return CalibInfo{
        // stupid c++ promoting my uint16_t to ints so we wrap .year in a cast
        .year = static_cast<uint16_t>((value & kYearMask) + 2010u),
        .month = static_cast<uint8_t>(value & kMonthMask),
        .day = static_cast<uint8_t>(value & kDayMask),
        .p_mode = static_cast<PMode>(value & kPModeMask)};
  }

  struct RawLdMeasurement {
    uint16_t pressure;
    uint16_t temperature;
  };

  struct LdMeasurement {
    double pressure;
    double temperature;
  };

  [[nodiscard]] std::expected<RawLdMeasurement, std::error_code> read_raw() noexcept
  {
    auto write_result = bus_->write(kI2cAddr, {&kRequestMeasurement, 1});
    if (!write_result)
      return std::unexpected(write_result.error());

    rclcpp::sleep_for(std::chrono::milliseconds(8));

    std::array<uint8_t, 5> measurement_data;
    auto read_result = bus_->read(kI2cAddr, measurement_data);

    if (!read_result)
      return std::unexpected(read_result.error());

    auto status_result = handle_status_byte(measurement_data[0]);
    if (!status_result)
      return std::unexpected(status_result.error());

    // stupid c++ promoting my uint16_t to ints so we wrap the entire thing in another cast
    return RawLdMeasurement{
        .pressure = static_cast<uint16_t>(
            (static_cast<uint16_t>(measurement_data[1]) << 8) | measurement_data[2]),
        .temperature = static_cast<uint16_t>(
            (static_cast<uint16_t>(measurement_data[3]) << 8) | measurement_data[4])};
  }

  [[nodiscard]] std::expected<LdMeasurement, std::error_code> read() noexcept
  {
    auto result = read_raw();
    if (!result)
      return std::unexpected(result.error());

    // cast to double so we can do our math at double precision
    const double pressure_min = static_cast<double>(pressure_min_);
    const double pressure_max = static_cast<double>(pressure_max_);

    // formulas from datasheet
    // https://web.archive.org/web/20251231045705/https://www.keller-druck2.ch/swupdate/InstallerD-LineAddressManager/manual/Communication_Protocol_4LD-9LD_en.pdf
    return LdMeasurement{
        .pressure = static_cast<double>(result.value().pressure - 16384) *
                        (pressure_max - pressure_min) / 32768 +
                    pressure_min_,
        .temperature = (((result.value().temperature >> 4) - 24) * 0.05) - 50};
  }

private:
  [[nodiscard]] inline std::expected<uint16_t, std::error_code>
  read_register(uint8_t mtp_addr) noexcept
  {
    auto write_result = bus_->write(kI2cAddr, {{mtp_addr, 1}});

    if (!write_result)
      return std::unexpected(write_result.error());

    rclcpp::sleep_for(std::chrono::microseconds(600));

    std::array<uint8_t, 3> register_value;
    auto read_result = bus_->read(kI2cAddr, register_value);

    if (!read_result)
      return std::unexpected(read_result.error());

    auto status_result = handle_status_byte(register_value[0]);
    if (!status_result)
      return std::unexpected(status_result.error());

    return (static_cast<uint16_t>(register_value[1]) << 8) | register_value[2];
  }

  [[nodiscard]] static inline std::expected<void, std::error_code>
  handle_status_byte(uint8_t status_byte) noexcept
  {
    if ((status_byte & kMemoryErrMask) != 0)
      return std::unexpected(LdErrorCode::memory_error);

    if ((status_byte & kBusyMask) != 0)
      return std::unexpected(LdErrorCode::response_timed_out);

    return {};
  }

  explicit LdDriver(std::shared_ptr<I2cBus> bus) : bus_(std::move(bus)) {}

  static constexpr uint8_t kI2cAddr = 0x40;
  static constexpr uint8_t kBusyMask = 0x20;
  static constexpr uint8_t kMemoryErrMask = 0x04;
  static constexpr uint8_t kRequestMeasurement = 0xAC;

  static constexpr uint8_t kScaling0Addr = 0x12;
  static constexpr uint8_t kScaling1Addr = 0x13;
  static constexpr uint8_t kScaling2Addr = 0x14;
  static constexpr uint8_t kScaling3Addr = 0x15;
  static constexpr uint8_t kScaling4Addr = 0x16;

  static constexpr uint8_t kYearShift = 11;
  static constexpr uint8_t kMonthShift = 7;
  static constexpr uint8_t kDayShift = 2;
  static constexpr uint8_t kPModeShift = 0;

  static constexpr uint16_t kYearMask = 0x001F;
  static constexpr uint16_t kMonthMask = 0x000F;
  static constexpr uint16_t kDayMask = 0x001F;
  static constexpr uint16_t kPModeMask = 0x0003;

  float pressure_min_;
  float pressure_max_;
  std::shared_ptr<I2cBus> bus_;
};
