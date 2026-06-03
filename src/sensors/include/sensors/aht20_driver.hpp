#include <expected>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>

#include "rclcpp/rclcpp.hpp"

#include "i2c_utils/i2c_bus.hpp"

enum class Aht20ErrorCode : uint8_t {
  measurement_timed_out = 1,
  crc_mismatch,
  invalid_measure_timeout,
  invalid_measure_wait_period,
};

class Aht20ErrorCategory : public std::error_category {
public:
  [[nodiscard]] const char *name() const noexcept override { return "Aht20"; }
  [[nodiscard]] std::string message(int ev) const noexcept override
  {
    switch (static_cast<Aht20ErrorCode>(ev)) {
      case Aht20ErrorCode::measurement_timed_out:
        return "Measurement did not return successfully within specified timeout period!";
      case Aht20ErrorCode::crc_mismatch: return "Recieved measurement data failed crc check!";
      case Aht20ErrorCode::invalid_measure_timeout:
        return "Measuremnet timeout values must be at least 80ms. Provided value was below this "
               "threshold.";
      case Aht20ErrorCode::invalid_measure_wait_period:
        return "Measurement wait period must be greater than 0!";
      default: return "Unknown Aht20 error!";
    }
  }
};

inline const Aht20ErrorCategory &aht20_error_category()
{
  static Aht20ErrorCategory instance;
  return instance;
}

inline std::error_code make_error_code(Aht20ErrorCode e) noexcept
{ return {static_cast<int>(e), aht20_error_category()}; }

namespace std {
template <> struct is_error_code_enum<Aht20ErrorCode> : true_type {};
} // namespace std

struct RawAht20Measurement {
  uint32_t temperature;
  uint32_t relative_humidity;
};

struct Aht20Measurement {
  double temperature;
  double relative_humidity;
};

class Aht20Driver {
public:
  Aht20Driver(const Aht20Driver &) = delete;
  Aht20Driver &operator=(const Aht20Driver &) = delete;

  [[nodiscard]] static std::expected<std::unique_ptr<Aht20Driver>, std::error_code> create(
      std::shared_ptr<I2cBus> bus,
      uint16_t measure_timeout_ms,
      uint8_t measure_wait_period_ms) noexcept
  {
    if (measure_timeout_ms < 80)
      return std::unexpected(Aht20ErrorCode::invalid_measure_timeout);
    if (measure_wait_period_ms == 0)
      return std::unexpected(Aht20ErrorCode::invalid_measure_wait_period);

    auto driver = std::unique_ptr<Aht20Driver>(
        new Aht20Driver(std::move(bus), measure_timeout_ms, measure_wait_period_ms));

    // sleep until 40ms after power up
    rclcpp::Clock clock(RCL_STEADY_TIME);
    clock.sleep_until(rclcpp::Time(40'000'000LL, RCL_STEADY_TIME));

    uint8_t status;
    auto result = driver->bus_->transfer(kI2cAddr, {&kStatusCommand, 1}, {&status, 1});
    if (!result)
      return std::unexpected(result.error());

    // if uncalibrated
    if ((status & kCalibEnabledMask) == 0) {
      auto result = driver->bus_->write(kI2cAddr, kInitCommandMessage);
      if (!result)
        return std::unexpected(result.error());

      rclcpp::sleep_for(std::chrono::milliseconds(10));
    }

    return driver;
  }

  [[nodiscard]] std::expected<RawAht20Measurement, std::error_code> read_raw() noexcept
  {
    auto result = bus_->write(kI2cAddr, kTriggerMeasurementMessage);
    if (!result)
      return std::unexpected(result.error());

    rclcpp::sleep_for(std::chrono::milliseconds(80));

    std::array<uint8_t, 7> measurement_data;
    uint16_t elapsed_ms = 80;
    bool timed_out = true;
    while (true) {
      auto result = bus_->read(kI2cAddr, measurement_data);
      if (!result)
        return std::unexpected(result.error());

      // if measurement completed
      if ((measurement_data[0] & kMeasurementBusyMask) == 0) {
        timed_out = false;
        break;
      }

      rclcpp::sleep_for(std::chrono::milliseconds(measure_wait_period_ms_));
      elapsed_ms += measure_wait_period_ms_;
      if (elapsed_ms >= measure_timeout_ms_)
        break;
    }

    if (timed_out) {
      return std::unexpected(Aht20ErrorCode::measurement_timed_out);
    }

    // loop over all bytes before the crc byte
    uint8_t crc = kCrcInitVal;
    for (uint8_t byte_idx = 0; byte_idx < measurement_data.size() - 1; ++byte_idx) {
      crc ^= measurement_data[byte_idx];
      for (uint8_t bit_idx = 0; bit_idx < 8; ++bit_idx) {
        if (crc & 0x80)
          crc = (crc << 1) ^ kCrcPolynomial;
        else
          crc <<= 1;
      }
    }

    if (crc != measurement_data[6])
      return std::unexpected(make_error_code(Aht20ErrorCode::crc_mismatch));

    // promote to uint32_t in one go to avoid a billion static casts
    const uint32_t b1 = measurement_data[1], b2 = measurement_data[2], b3 = measurement_data[3],
                   b4 = measurement_data[4], b5 = measurement_data[5];

    // Temperature and humidity are both 20 bits so we pack it into two uint32_ts
    return RawAht20Measurement{
        .temperature = (b1 << 12) | (b2 << 4) | (b3 >> 4),
        .relative_humidity = ((b3 & 0x0F) << 16) | (b4 << 8) | b5};
  }

  [[nodiscard]] std::expected<Aht20Measurement, std::error_code> read() noexcept
  {
    auto result = read_raw();
    if (!result)
      return std::unexpected(result.error());

    return Aht20Measurement{
        // see datasheet
        .temperature = result.value().temperature / 1048576.0 * 200.0 - 50.0,
        .relative_humidity = result.value().relative_humidity / 1048576.0};
  }

private:
  explicit Aht20Driver(
      std::shared_ptr<I2cBus> bus,
      uint16_t measure_timeout_ms,
      uint8_t measure_wait_period_ms)
      : bus_(std::move(bus)), measure_timeout_ms_(measure_timeout_ms),
        measure_wait_period_ms_(measure_wait_period_ms)
  {
  }

  static constexpr uint8_t kI2cAddr = 0x38;
  static constexpr uint8_t kStatusCommand = 0x71;
  static constexpr uint8_t kCalibEnabledMask = (0x1 << 3);
  static constexpr uint8_t kMeasurementBusyMask = (0x01 << 7);
  static constexpr std::array<uint8_t, 3> kInitCommandMessage{0xBE, 0x08, 0x00};
  static constexpr std::array<uint8_t, 3> kTriggerMeasurementMessage{0xAC, 0x33, 0x00};
  static constexpr uint8_t kCrcInitVal = 0xFF;
  static constexpr uint8_t kCrcPolynomial = 0x31;

  std::shared_ptr<I2cBus> bus_;
  uint16_t measure_timeout_ms_;
  uint8_t measure_wait_period_ms_;
};
