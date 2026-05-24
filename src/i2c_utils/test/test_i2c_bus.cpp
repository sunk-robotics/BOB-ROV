#include "i2c_utils/i2c_bus.hpp"
#include <cstdio>

int main()
{
  auto bus = I2cBus::get_instance(I2cBus::BusNum::bus_1);
  if (!bus) {
    std::printf("Failed to open bus: %s\n", bus.error().message().c_str());
    return 1;
  }

  uint8_t write_buf[] = {0xB0, 0xA0};
  auto write_result = (*bus)->write(0x28, write_buf);
  if (!write_result) {
    std::printf("Write failed: %s\n", write_result.error().message().c_str());
    return 1;
  }
}
