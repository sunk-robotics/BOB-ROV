#include <format>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"

#include "control/motor_controller_driver.hpp"
#include "spi_utils/spi_bus.hpp"

namespace control {
class MotorControllerNode : public rclcpp::Node {
public:
  MotorControllerNode(const rclcpp::NodeOptions &options) : Node("motor_controller_node", options)
  {
    rcl_interfaces::msg::ParameterDescriptor motor_pole_pairs_desc;
    rcl_interfaces::msg::IntegerRange motor_pole_pairs_range;
    motor_pole_pairs_range.from_value = 1;
    motor_pole_pairs_range.to_value = 255;
    motor_pole_pairs_range.step = 1;
    motor_pole_pairs_desc.integer_range = {motor_pole_pairs_range};
    this->declare_parameter("motor_pole_pairs", kDefaultMotorPolePairs, motor_pole_pairs_desc);

    rcl_interfaces::msg::ParameterDescriptor bus_desc;
    rcl_interfaces::msg::IntegerRange bus_range;
    bus_range.from_value = 0;
    bus_range.to_value = SpiBus::kMaxBusNum;
    bus_range.step = 1;
    bus_desc.integer_range = {bus_range};
    this->declare_parameter("spi_bus", kDefaultSpiBus, bus_desc);

    rcl_interfaces::msg::ParameterDescriptor cs_desc;
    rcl_interfaces::msg::IntegerRange cs_range;
    cs_range.from_value = 0;
    cs_range.to_value = SpiBus::kMaxCsNum;
    cs_range.step = 1;
    cs_desc.integer_range = {cs_range};
    this->declare_parameter("cs", kDefaultCs);

    rcl_interfaces::msg::ParameterDescriptor spi_speed_desc;
    rcl_interfaces::msg::IntegerRange spi_speed_range;
    spi_speed_range.from_value = 1000;
    spi_speed_range.to_value = 1'000'000;
    spi_speed_range.step = 1;
    spi_speed_desc.integer_range = {spi_speed_range};

    this->declare_parameter("throttle_topic", kDefaultThrottleTopic);
    this->declare_parameter("rpm_topic", kDefaultRpmTopic);

    const uint8_t pole_pairs = this->get_parameter("motor_pole_pairs").as_int();

    const auto bus_num = static_cast<SpiBus::BusNum>(this->get_parameter("spi_bus").as_int());
    const uint8_t cs = this->get_parameter("cs").as_int();
    const uint32_t spi_speed = this->get_parameter("spi_speed").as_int();

    const auto throttle_topic = this->get_parameter("throttle_topic").as_string();
    const auto rpm_topic = this->get_parameter("rpm_topic").as_string();
    RCLCPP_INFO(get_logger(), "Loaded motor_controller_node params");

    auto bus = SpiBus::get_instance(bus_num, cs);
    if (!bus) {
      auto msg = std::format("Failed to initialize SPI bus! Error: {}", bus.error().message());
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::runtime_error(msg);
    }

    auto driver_res = MotorControllerDriver::create(bus.value(), spi_speed, pole_pairs);
    if (!driver_res) {
      auto msg = std::format(
          "Failed to initialize motor controller driver! Error: {}", driver_res.error().message());
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::runtime_error(msg);
    }
  }

private:
  std::unique_ptr<MotorControllerDriver> driver_;
  // corect value for T200
  // see https://discuss.bluerobotics.com/t/full-caracteristics-of-the-t100-and-t200/5226/6
  static constexpr uint8_t kDefaultMotorPolePairs = 7;
  static constexpr uint8_t kDefaultSpiBus = 0;
  static constexpr uint8_t kDefaultCs = 0;

  static constexpr char kDefaultThrottleTopic[] = "motor/throttle";
  static constexpr char kDefaultRpmTopic[] = "motor/RPM";
};
} // namespace control

RCLCPP_COMPONENTS_REGISTER_NODE(control::MotorControllerNode);
