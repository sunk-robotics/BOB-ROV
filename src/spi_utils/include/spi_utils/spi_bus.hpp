#pragma once

#include <cstring>
#include <expected>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <system_error>

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

#include "spi_err.hpp"

#ifdef SPI_UTILS_ROS_LOGGING

#  include <rclcpp/logging.hpp>
#  define SPI_BUS_LOG_ERROR(fmt, ...) RCLCPP_ERROR(rclcpp::get_logger("SpiBus"), fmt, ##__VA_ARGS__)
#else
#  include <cstdio>
#  define SPI_BUS_LOG_ERROR(fmt, ...)                                                              \
    std::fprintf(stderr, "[SpiBus] ERROR: " fmt "\n", ##__VA_ARGS__)
#endif

class SpiBus {
public:
  SpiBus(const SpiBus &) = delete;
  SpiBus(SpiBus &&) = delete;
  SpiBus &operator=(const SpiBus &) = delete;
  SpiBus &operator=(SpiBus &&) = delete;

  enum class Bus { Spi0, Spi1 };

  [[nodiscard]] static std::expected<std::shared_ptr<SpiBus>, std::error_code>
  get_instance(Bus bus, uint8_t cs) noexcept
  {
    static std::mutex registry_mutex;
    static std::map<std::pair<Bus, uint8_t>, std::weak_ptr<SpiBus>> cs_registry;
    static std::map<Bus, std::shared_ptr<std::mutex>> bus_mutexes;

    std::lock_guard lock(registry_mutex);

    auto path = get_path(bus, cs);
    if (!path) {
      return std::unexpected(path.error());
    }

    auto key = std::make_pair(bus, cs);
    auto &weak = cs_registry[key];
    if (weak.lock()) {
      return std::unexpected(make_error_code(SpiErrorCode::cs_in_use));
    }

    // get mutex pointer
    auto [it, inserted] = bus_mutexes.emplace(bus, nullptr);
    if (inserted) {
      it->second = std::make_shared<std::mutex>();
    };

    // returns -1 if err
    int fd = open(path.value(), O_RDWR);
    if (fd < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }

    auto shared = std::shared_ptr<SpiBus>(new SpiBus(fd, it->second));
    weak = shared;
    return shared;
  };

  [[nodiscard]] std::expected<void, std::error_code>
  transaction(std::span<spi_ioc_transfer> msgs) noexcept
  {
    // 0 is ripped from SPI_IOC_MESSAGE implemetnation
    uint32_t request = _IOC(_IOC_WRITE, SPI_IOC_MAGIC, 0, msgs.size() * sizeof(spi_ioc_transfer));

    std::lock_guard lock(*bus_mutex_);
    if (ioctl(fd_, request, msgs.data()) < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }
    return {};
  };

  [[nodiscard]] std::expected<void, std::error_code>
  transfer(std::span<uint8_t> read_buf, std::span<const uint8_t> write_buf) noexcept
  {
    if (read_buf.size() != write_buf.size()) {
      return std::unexpected(
          std::error_code(make_error_code(SpiErrorCode::missmatched_transfer_bus_size)));
    }

    // zero all fields; 0 is used as use default as set by configure()
    spi_ioc_transfer transfer{};
    transfer.tx_buf = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(write_buf.data()));
    transfer.rx_buf = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(read_buf.data()));
    transfer.len = static_cast<uint32_t>(write_buf.size());

    return transaction({&transfer, 1});
  }

  [[nodiscard]] std::expected<void, std::error_code> read(std::span<uint8_t> read_buf) noexcept
  {
    spi_ioc_transfer transfer{};
    transfer.tx_buf = 0;
    transfer.rx_buf = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(read_buf.data()));
    transfer.len = static_cast<uint32_t>(read_buf.size());

    return transaction({&transfer, 1});
  }

  [[nodiscard]] std::expected<void, std::error_code>
  write(std::span<const uint8_t> write_buf) noexcept
  {
    spi_ioc_transfer transfer{};
    transfer.tx_buf = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(write_buf.data()));
    transfer.rx_buf = 0;
    transfer.len = static_cast<uint32_t>(write_buf.size());

    return transaction({&transfer, 1});
  }

  [[nodiscard]] std::expected<void, std::error_code>
  configure(uint8_t mode_flags, uint8_t bits, uint32_t speed) noexcept
  {
    if (ioctl(fd_, SPI_IOC_WR_MODE, &mode_flags) < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }
    if (ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }
    if (ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, std::error_code> close() noexcept
  {
    if (fd_ < 0)
      return {};

    if (::close(fd_) < 0) {
      return std::unexpected(std::error_code(errno, std::system_category()));
    }
    fd_ = -1;
    return {};
  }

  ~SpiBus()
  {
    if (fd_ >= 0) {
      if (::close(fd_) < 0) {
        SPI_BUS_LOG_ERROR("Failed to close SPI bus fd %d: %s", fd_, std::strerror(errno));
      }
      fd_ = -1;
    }
  }

private:
  static std::expected<const char *, std::error_code> get_path(Bus bus, uint8_t cs) noexcept
  {
    static constexpr std::array<const char *, 2> spi0_paths{"/dev/spidev0.0", "/dev/spidev0.1"};
    static constexpr std::array<const char *, 3> spi1_paths{
        "/dev/spidev1.0", "/dev/spidev1.1", "/dev/spidev1.2"};

    switch (bus) {
    case Bus::Spi0:
      if (cs < spi0_paths.size()) {
        return spi0_paths[cs];
      } else {
        return std::unexpected(make_error_code(SpiErrorCode::invalid_cs));
      }
    case Bus::Spi1:
      if (cs < spi1_paths.size()) {
        return spi1_paths[cs];
      } else {
        return std::unexpected(make_error_code(SpiErrorCode::invalid_cs));
      }
    }
    return std::unexpected(make_error_code(SpiErrorCode::invalid_bus));
  };

  explicit SpiBus(int fd, std::shared_ptr<std::mutex> bus_mutex)
      : fd_(fd), bus_mutex_(bus_mutex) {};
  int fd_ = -1;
  std::shared_ptr<std::mutex> bus_mutex_;
};
