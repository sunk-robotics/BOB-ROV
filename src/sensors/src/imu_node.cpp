#include <chrono>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/magnetic_field.hpp"

#include "i2c_utils/i2c_bus.hpp"
#include "sensors/bno055_driver.hpp"

using namespace std::chrono_literals;

namespace sensors {

class ImuNode : public rclcpp::Node {
public:
  ImuNode(const rclcpp::NodeOptions &options) : Node("imu_node", options)
  {
    rcl_interfaces::msg::ParameterDescriptor pub_rate_desc;
    rcl_interfaces::msg::IntegerRange pub_rate_range;
    pub_rate_range.from_value = 1;
    pub_rate_range.to_value = 400;
    pub_rate_range.step = 1;
    pub_rate_desc.integer_range = {pub_rate_range};
    this->declare_parameter("pub_rate_hz", kDefaultPubRateHz, pub_rate_desc);

    rcl_interfaces::msg::ParameterDescriptor bus_desc;
    rcl_interfaces::msg::IntegerRange bus_range;
    bus_range.from_value = 0;
    bus_range.to_value = I2cBus::MaxBusNum;
    bus_range.step = 1;
    bus_desc.integer_range = {bus_range};
    this->declare_parameter("i2c_bus", 1, bus_desc);

    this->declare_parameter("i2c_bus", 1);
    this->declare_parameter("use_alternate_i2c_addr", false);

    this->declare_parameter("enable_orientation", false);
    this->declare_parameter("enable_accel", true);
    this->declare_parameter("enable_gyro", true);
    this->declare_parameter("enable_mag", true);

    rcl_interfaces::msg::ParameterDescriptor var_desc;
    rcl_interfaces::msg::FloatingPointRange var_range;
    var_range.from_value = 0.0;
    var_range.to_value = std::numeric_limits<double>::max();
    var_range.step = 0.0;
    var_desc.floating_point_range = {var_range};
    this->declare_parameter("accel_variance", kDefaultAccelVar, var_desc);
    this->declare_parameter("gyro_variance", kDefaultGyroVar, var_desc);
    this->declare_parameter("mag_variance", kDefaultMagVar, var_desc);

    const auto pub_rate_hz = this->get_parameter("pub_rate_hz").as_int();

    const auto bus_num = static_cast<I2cBus::BusNum>(this->get_parameter("i2c_bus").as_int());

    const bool use_alternate_addr = this->get_parameter("use_alternate_i2c_addr").as_bool();

    enable_orientation_ = this->get_parameter("enable_orientation").as_bool();
    enable_accel_ = this->get_parameter("enable_accel").as_bool();
    enable_gyro_ = this->get_parameter("enable_gyro").as_bool();
    enable_mag_ = this->get_parameter("enable_mag").as_bool();
    imu_msg_enable_ = enable_orientation_ || enable_accel_ || enable_gyro_;

    const double accel_var = this->get_parameter("accel_variance").as_double();
    const double gyro_var = this->get_parameter("gyro_variance").as_double();
    const double mag_var = this->get_parameter("mag_variance").as_double();

    imu_publisher_ = this->create_publisher<sensor_msgs::msg::Imu>("imu/data_raw", 10);
    mag_publisher_ = this->create_publisher<sensor_msgs::msg::MagneticField>("imu/mag", 10);

    // clang-format off
    accel_covariance_ = {
      accel_var, 0, 0,
      0, accel_var, 0,
      0, 0, accel_var
    };

    gyro_covariance_ = {
      gyro_var, 0, 0,
      0, gyro_var, 0,
      0, 0, gyro_var
    };

    mag_covariance_ = {
      mag_var, 0, 0,
      0, mag_var, 0, 
      0, 0, mag_var
    };
    // clang-format on

    auto bus = I2cBus::get_instance(bus_num);
    if (!bus) {
      auto msg = std::format("Failed to get I2C bus handle! Error: {}", bus.error().message());
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::runtime_error(msg);
    }

    u8 op_mode;
    if (enable_orientation_) {
      op_mode = BNO055_OPERATION_MODE_NDOF;
    } else {
      uint8_t idx = (enable_accel_ << 2) | (enable_mag_ << 1) | enable_gyro_;
      op_mode = kSensorModeTable[idx];
    }

    auto sensor_res = Bno055Driver::create(bus.value(), use_alternate_addr, op_mode);
    if (!sensor_res) {
      auto msg =
          std::format("Failed to initialize IMU sensor! Error: {}", sensor_res.error().message());
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::runtime_error(msg);
    }
    sensor_ = std::move(sensor_res.value());

    auto timer_callback = [this]() -> void {
      if (imu_msg_enable_) {
        bool imu_ok = true;
        sensor_msgs::msg::Imu imu_msg;

        if (enable_orientation_ &&
            bno055_read_quaternion_wxyz(&orientation_vals_) == BNO055_ERROR) {
          RCLCPP_ERROR(get_logger(), "Error encountered while reading IMU orientation data!");
          imu_ok = false;
        }
        if (imu_ok && enable_accel_ && bno055_read_accel_xyz(&accel_vals_) == BNO055_ERROR) {
          RCLCPP_ERROR(get_logger(), "Error encountered while reading IMU accelerometer!");
          imu_ok = false;
        }
        if (imu_ok && enable_gyro_ && bno055_read_gyro_xyz(&gyro_vals_) == BNO055_ERROR) {
          RCLCPP_ERROR(get_logger(), "Error encountered while reading IMU gyroscope!");
          imu_ok = false;
        }

        if (imu_ok) {
          imu_msg.header.stamp = this->now();
          imu_msg.header.frame_id = "imu_link";

          if (enable_orientation_) {
            imu_msg.orientation.x = orientation_vals_.x;
            imu_msg.orientation.y = orientation_vals_.y;
            imu_msg.orientation.z = orientation_vals_.z;
            imu_msg.orientation.w = orientation_vals_.w;
          } else {
            imu_msg.orientation_covariance = orientation_unavailable;
          }
          if (enable_accel_) {
            imu_msg.linear_acceleration.x = accel_vals_.x;
            imu_msg.linear_acceleration.y = accel_vals_.y;
            imu_msg.linear_acceleration.z = accel_vals_.z;
          }
          if (enable_gyro_) {
            imu_msg.angular_velocity.x = gyro_vals_.x;
            imu_msg.angular_velocity.y = gyro_vals_.y;
            imu_msg.angular_velocity.z = gyro_vals_.z;
          }
          imu_msg.linear_acceleration_covariance = accel_covariance_;
          imu_msg.angular_velocity_covariance = gyro_covariance_;
          this->imu_publisher_->publish(imu_msg);
        }
      }

      if (enable_mag_) {
        if (bno055_read_mag_xyz(&mag_vals_) == BNO055_ERROR) {
          RCLCPP_ERROR(get_logger(), "Error encountered while reading IMU magnometer!");
        } else {
          sensor_msgs::msg::MagneticField mag_msg;
          mag_msg.header.stamp = this->now();
          mag_msg.header.frame_id = "imu_link";
          mag_msg.magnetic_field.x = static_cast<double>(mag_vals_.x) * 1e-6;
          mag_msg.magnetic_field.y = static_cast<double>(mag_vals_.y) * 1e-6;
          mag_msg.magnetic_field.z = static_cast<double>(mag_vals_.z) * 1e-6;
          mag_msg.magnetic_field_covariance = mag_covariance_;
          this->mag_publisher_->publish(mag_msg);
        }
      }
    };

    auto period = std::chrono::duration<double>(1.0 / pub_rate_hz);
    timer_ = this->create_wall_timer(period, timer_callback);
  }

private:
  bool enable_orientation_;
  bool enable_accel_;
  bool enable_gyro_;
  bool enable_mag_;

  bool imu_msg_enable_;

  std::array<double, 9> accel_covariance_;
  std::array<double, 9> gyro_covariance_;
  std::array<double, 9> mag_covariance_;

  bno055_accel_t accel_vals_;
  bno055_gyro_t gyro_vals_;
  bno055_mag_t mag_vals_;
  bno055_quaternion_t orientation_vals_;

  static constexpr uint16_t kDefaultPubRateHz = 100;

  // g/sqrt(Hz) => m/s^2 / sqrt(Hz) from datasheet
  static constexpr double kAccelNoiseDensity = 0.00015 * 9.80665;
  // (m/s^2 / sqrt(Hz))^2 = (m/s^2)^2/Hz => (m/s^2)^2/Hz * Hz = variance
  static constexpr double kDefaultAccelVar =
      kAccelNoiseDensity * kAccelNoiseDensity * kDefaultPubRateHz;

  // deg/s / sqrt(Hz) => rad/s / sqrt(Hz) from datasheet
  static constexpr double kGyroNoiseDensity = 0.01 * 3.14159 / 180;
  // (rad/s / sqrt(Hz))^2 = (rad/s)^2/(Hz) => (rads/s)^2/Hz * Hz = variance
  static constexpr double kDefaultGyroVar =
      kGyroNoiseDensity * kGyroNoiseDensity * kDefaultPubRateHz;

  static constexpr double kDefaultMagVar = 9e-14; // resolution ^2

  // clang-format off
  static constexpr std::array<double, 9> orientation_unavailable = {
    -1, 0, 0,
    0, 0, 0,
    0, 0, 0
  };
  // clang-format on

  static constexpr std::array<u8, 8> kSensorModeTable{
      0,                             // 000 - none (invalid)
      BNO055_OPERATION_MODE_GYRONLY, // 001
      BNO055_OPERATION_MODE_MAGONLY, // 010
      BNO055_OPERATION_MODE_MAGGYRO, // 011
      BNO055_OPERATION_MODE_ACCONLY, // 100
      BNO055_OPERATION_MODE_ACCGYRO, // 101
      BNO055_OPERATION_MODE_ACCMAG,  // 110
      BNO055_OPERATION_MODE_AMG,     // 111
  };

  std::unique_ptr<Bno055Driver> sensor_;

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::MagneticField>::SharedPtr mag_publisher_;
};

} // namespace sensors

RCLCPP_COMPONENTS_REGISTER_NODE(sensors::ImuNode);
