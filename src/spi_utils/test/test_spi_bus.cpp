#include "spi_utils/spi_bus.hpp"
#include <cstdio>

int main()
{
  auto bus = SpiBus::get_instance(SpiBus::Bus::Spi0, 0);
  if (!bus) {
    std::printf("Failed to open bus: %s\n", bus.error().message().c_str());
    return 1;
  }

  uint8_t write_buf[] = {0xB0, 0xA0};
  uint8_t read_buf[2] = {};
  auto result = (*bus)->transfer(read_buf, write_buf);
  if (!result) {
    std::printf("Transfer failed: %s\n", result.error().message().c_str());
    return 1;
  }
}
