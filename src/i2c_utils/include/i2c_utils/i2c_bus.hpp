#pragma once

#include <cstring>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <system_error>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef I2C_UTILS_ROS_LOGGING
#  include <rclcpp/logging.hpp>
#  define I2C_BUS_LOG_ERROR(fmt, ...) RCLCPP_ERROR(rclcpp::get_logger("I2cBus"), fmt, ##__VA_ARGS__)
#else
#  include <cstdio>
#  define I2C_BUS_LOG_ERROR(fmt, ...)                                                              \
    std::fprintf(stderr, "[I2cBus] ERROR: " fmt "\n", ##__VA_ARGS__)
#endif

// Then in the destructor, just:

class I2cBus {
public:
  I2cBus(const I2cBus &) = delete;
  I2cBus(I2cBus &&) = delete;
  I2cBus &operator=(const I2cBus &) = delete;
  I2cBus &operator=(I2cBus &&) = delete;

  enum class BusNum : uint8_t { bus_0 = 0, bus_1 = 1 };
  static constexpr uint8_t MaxBusNum = 1;

  [[nodiscard]] static std::expected<std::shared_ptr<I2cBus>, std::error_code>
  get_instance(BusNum bus_num) noexcept
  {
    static std::mutex registry_mutex;
    static std::array<std::weak_ptr<I2cBus>, kBusPaths.size()> bus_registry;

    std::lock_guard lock(registry_mutex);

    auto idx = static_cast<uint8_t>(bus_num);

    // returns weak_ptr<I2cBus>(), if key does not yet exist
    auto &weak = bus_registry[idx];

    if (std::shared_ptr<I2cBus> shared = weak.lock()) {
      return shared;
    }

    // returns -1 if err
    int fd = open(kBusPaths[idx], O_RDWR);
    if (fd < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }

    auto shared = std::shared_ptr<I2cBus>(new I2cBus(fd)); // creates new bus
    weak = shared;                                         // Add to registry map
    return shared;
  }

  [[nodiscard]] std::expected<void, std::error_code> transaction(std::span<i2c_msg> msgs) noexcept
  {
    i2c_rdwr_ioctl_data data{.msgs = msgs.data(), .nmsgs = static_cast<uint32_t>(msgs.size())};

    std::lock_guard lock(bus_mutex_);
    if (ioctl(fd_, I2C_RDWR, &data) < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, std::error_code>
  transfer(uint8_t addr, std::span<const uint8_t> write_buf, std::span<uint8_t> read_buf) noexcept
  {
    i2c_msg msgs[2]{
        {.addr = addr,
         .flags = 0,
         .len = static_cast<uint16_t>(write_buf.size()),
         .buf = const_cast<uint8_t *>(write_buf.data())},
        {.addr = addr,
         .flags = I2C_M_RD,
         .len = static_cast<uint16_t>(read_buf.size()),
         .buf = read_buf.data()},
    };

    return transaction(msgs);
  }

  [[nodiscard]] std::expected<void, std::error_code>
  read(uint8_t addr, std::span<uint8_t> read_buf) noexcept
  {
    i2c_msg msg{
        .addr = addr,
        .flags = I2C_M_RD,
        .len = static_cast<uint16_t>(read_buf.size()),
        .buf = read_buf.data()};

    return transaction({&msg, 1});
  }

  [[nodiscard]] std::expected<void, std::error_code>
  write(uint8_t addr, std::span<const uint8_t> write_buf) noexcept
  {
    i2c_msg msg{
        .addr = addr,
        .flags = 0,
        .len = static_cast<uint16_t>(write_buf.size()),
        .buf = const_cast<uint8_t *>(write_buf.data())};

    return transaction({&msg, 1});
  }

  [[nodiscard]] std::expected<void, std::error_code> close() noexcept
  {
    std::lock_guard lock(bus_mutex_);
    if (fd_ < 0)
      return {};

    if (::close(fd_) < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }
    fd_ = -1;
    return {};
  }

  ~I2cBus()
  {
    if (fd_ >= 0) {
      if (::close(fd_) < 0) {
        I2C_BUS_LOG_ERROR("Failed to close I2C bus fd %d: %s", fd_, std::strerror(errno));
      }
      fd_ = -1;
    }
  }

private:
  static constexpr std::array<const char *, 2> kBusPaths{"/dev/i2c-0", "/dev/i2c-1"};
  explicit I2cBus(int fd) : fd_(fd) {};
  int fd_ = -1;
  std::mutex bus_mutex_;
};
