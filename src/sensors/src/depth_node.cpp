#include <format>
#include <stdexcept>
#include <string>

#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/fluid_pressure.hpp"
#include "sensor_msgs/msg/temperature.hpp"

#include "i2c_utils/i2c_bus.hpp"
#include "sensors/ld_driver.hpp"

namespace sensors {
class DepthSensorNode : public rclcpp::Node {
public:
  DepthSensorNode(const rclcpp::NodeOptions &options) : Node("depth_node", options)
  {
    rcl_interfaces::msg::ParameterDescriptor pub_rate_desc;
    rcl_interfaces::msg::IntegerRange pub_rate_range;
    pub_rate_range.from_value = 1;
    pub_rate_range.to_value = 100;
    pub_rate_range.step = 1;
    pub_rate_desc.integer_range = {pub_rate_range};
    this->declare_parameter("pub_rate_hz", kDefaultPubRateHz, pub_rate_desc);

    rcl_interfaces::msg::ParameterDescriptor bus_desc;
    rcl_interfaces::msg::IntegerRange bus_range;
    bus_range.from_value = 0;
    bus_range.to_value = I2cBus::kMaxBusNum;
    bus_range.step = 1;
    bus_desc.integer_range = {bus_range};
    this->declare_parameter("i2c_bus", kDefaultI2cBus, bus_desc);

    rcl_interfaces::msg::ParameterDescriptor pressure_var_desc;
    rcl_interfaces::msg::FloatingPointRange pressure_var_range;
    pressure_var_range.from_value = 0.0;
    pressure_var_range.to_value = 1.0;
    pressure_var_range.step = 0.0;
    pressure_var_desc.floating_point_range = {pressure_var_range};
    this->declare_parameter("pressure_variance", kDefaultPressureVar, pressure_var_desc);

    rcl_interfaces::msg::ParameterDescriptor temp_var_desc;
    rcl_interfaces::msg::FloatingPointRange temp_var_range;
    temp_var_range.from_value = 0.0;
    temp_var_range.to_value = std::numeric_limits<double>::max();
    temp_var_range.step = 0.0;
    temp_var_desc.floating_point_range = {temp_var_range};
    this->declare_parameter("temp_variance", kDefaultTempVar, temp_var_desc);

    this->declare_parameter("pressure_topic", kDefaultPressureTopic);
    this->declare_parameter("temperature_topic", kDefaultTempTopic);

    this->declare_parameter("link", kDefaultLink);

    const auto pub_rate_hz = this->get_parameter("pub_rate_hz").as_int();

    const auto bus_num = static_cast<I2cBus::BusNum>(this->get_parameter("i2c_bus").as_int());

    pressure_var_ = this->get_parameter("pressure_variance").as_double();
    temp_var_ = this->get_parameter("temperature_variance").as_double();

    const auto pressure_topic = this->get_parameter("pressure_topic").as_string();
    const auto temp_topic = this->get_parameter("temperature_topic").as_string();

    link_ = this->get_parameter("link").as_string();
    RCLCPP_INFO(get_logger(), "Loaded depth_node params");

    auto bus = I2cBus::get_instance(bus_num);
    if (!bus) {
      auto msg = std::format("Failed to get I2C bus handle! Error: {}", bus.error().message());
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::runtime_error(msg);
    }

    auto sensor_res = LdDriver::create(bus.value());
    if (!sensor_res) {
      auto msg =
          std::format("Failed to initialize depth sensor! Error: {}", sensor_res.error().message());
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::runtime_error(msg);
    }
    sensor_ = std::move(sensor_res.value());
    RCLCPP_INFO(get_logger(), "Initialized depth sensor!");

    auto period = std::chrono::duration<double>(1.0 / pub_rate_hz);
    timer_ = this->create_wall_timer(period, [this]() { timer_callback(); });
    RCLCPP_INFO(get_logger(), "Started depth sensor node!");
  }

private:
  void timer_callback() {}

  double pressure_var_;
  double temp_var_;
  std::string link_;

  static constexpr uint16_t kDefaultPubRateHz = 100;

  static constexpr uint8_t kDefualtI2cBus = 1;
  static constexpr char kDefaultPressureTopic[] = "depth/pressure";
  static constexpr char kDefaultTempTopic[] = "depth/temp";
  static constexpr char kDefaultLink[] = "depth_sensor_link";

  std::unique_ptr<LdDriver> sensor_;

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<sensor_msgs::msg::FluidPressure>::SharedPtr pressure_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::Temperature>::SharedPtr temp_publisher_;
};

} // namespace sensors

RCLCPP_COMPONENTS_REGISTER_NODE(sensors::DepthSensorNode);
