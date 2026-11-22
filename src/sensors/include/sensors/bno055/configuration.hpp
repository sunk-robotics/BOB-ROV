#pragma once

#include <cstdint>

extern "C"
{
#include "bno055.h"
}

namespace sensors::bno055
{

/// The two I2C addresses the BNO055 can respond on, selected by the COM3 pin.
enum class I2cAddress : uint8_t
{
  primary = BNO055_I2C_ADDR1,
  alternate = BNO055_I2C_ADDR2
};

/// The sensor fusion / sensor selection modes of the BNO055.
enum class OperationMode : uint8_t
{
  configuration = BNO055_OPERATION_MODE_CONFIG,
  accelerometer_only = BNO055_OPERATION_MODE_ACCONLY,
  magnetometer_only = BNO055_OPERATION_MODE_MAGONLY,
  gyroscope_only = BNO055_OPERATION_MODE_GYRONLY,
  accelerometer_magnetometer = BNO055_OPERATION_MODE_ACCMAG,
  accelerometer_gyroscope = BNO055_OPERATION_MODE_ACCGYRO,
  magnetometer_gyroscope = BNO055_OPERATION_MODE_MAGGYRO,
  accelerometer_magnetometer_gyroscope = BNO055_OPERATION_MODE_AMG,
  inertial_measurement_unit = BNO055_OPERATION_MODE_IMUPLUS,
  compass = BNO055_OPERATION_MODE_COMPASS,
  magnetometer_for_gyroscope = BNO055_OPERATION_MODE_M4G,
  nine_degrees_of_freedom_fast_magnetometer_calibration_off = BNO055_OPERATION_MODE_NDOF_FMC_OFF,
  nine_degrees_of_freedom = BNO055_OPERATION_MODE_NDOF
};

struct Configuration
{
  I2cAddress i2c_address = I2cAddress::primary;
  OperationMode operation_mode = OperationMode::nine_degrees_of_freedom;
};

}  // namespace sensors::bno055
