#include <format>
#include <stdexcept>
#include <utility>

#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/relative_humidity.hpp"
#include "sensor_msgs/msg/temperature.hpp"

#include "i2c_utils/i2c_bus.hpp"
#include "sensors/aht20_driver.hpp"

namespace sensors {

class TempHumidSensorNode : public rclcpp::Node {
public:
  TempHumidSensorNode(const rclcpp::NodeOptions &options) : Node("temp_humid_node", options)
  {
    rcl_interfaces::msg::ParameterDescriptor pub_rate_desc;
    rcl_interfaces::msg::IntegerRange pub_rate_range;
    pub_rate_range.from_value = 1;
    pub_rate_range.to_value = 10;
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

    this->declare_parameter("measurement_timeout_us", kDefaultMeasurementTimeout);
    this->declare_parameter("measurement_poll_period_us", kDefaultPollPeriod);

    rcl_interfaces::msg::ParameterDescriptor temp_var_desc;
    rcl_interfaces::msg::FloatingPointRange temp_var_range;
    temp_var_range.from_value = 0.0;
    temp_var_range.to_value = std::numeric_limits<double>::max();
    temp_var_range.step = 0.0;
    temp_var_desc.floating_point_range = {temp_var_range};
    this->declare_parameter("temp_variance", kDefaultTempVar, temp_var_desc);

    rcl_interfaces::msg::ParameterDescriptor humid_var_desc;
    rcl_interfaces::msg::FloatingPointRange humid_var_range;
    humid_var_range.from_value = 0.0;
    humid_var_range.to_value = 1.0;
    humid_var_range.step = 0.0;
    humid_var_desc.floating_point_range = {humid_var_range};
    this->declare_parameter("humid_variance", kDefaultHumidVar, humid_var_desc);

    this->declare_parameter("temperature_topic", "temphumid/temp");
    this->declare_parameter("humidity_topic", "temphumid/humid");

    this->declare_parameter("link", "temp_humid_sensor_link");

    const auto pub_rate_hz = this->get_parameter("pub_rate_hz").as_int();

    const auto bus_num = static_cast<I2cBus::BusNum>(this->get_parameter("i2c_bus").as_int());

    // limit maximum timeout to ensure that it doesn't exceed the update rate
    const auto max_timeout = 1'000'000 / pub_rate_hz;
    const auto measure_timeout = this->get_parameter("measurement_timeout_us").as_int();
    if (measure_timeout > max_timeout) {
      auto msg = std::format(
          "measurement_timeout_us ({}) must be <= 1000000 / pub_rate_hz ({})",
          measure_timeout,
          max_timeout);
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::invalid_argument(msg);
    }

    // limit poll period so that at least one poll happens after initial delay period of 80ms
    // mandated by the datasheet.
    const auto measure_wait_period = this->get_parameter("measurement_poll_period_us").as_int();
    if (measure_wait_period > measure_timeout - 80) {
      auto msg = std::format(
          "measurement_poll_period_us ({}) must be <= measurement_timeout_us - 80 ({})",
          measure_wait_period,
          measure_timeout - 80);
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::invalid_argument(msg);
    }

    temp_var_ = this->get_parameter("temp_variance").as_double();
    humid_var_ = this->get_parameter("humid_variance").as_double();

    const auto temp_topic = this->get_parameter("temperature_topic").as_string();
    const auto humid_topic = this->get_parameter("humidity_topic").as_string();

    link_ = this->get_parameter("link").as_string();
    RCLCPP_INFO(get_logger(), "Loaded temp_humid_node params");

    temp_publisher_ = this->create_publisher<sensor_msgs::msg::Temperature>(temp_topic, 10);
    humid_publisher_ = this->create_publisher<sensor_msgs::msg::RelativeHumidity>(humid_topic, 10);
    RCLCPP_INFO(get_logger(), "Created temp_humid_node publishers");

    auto bus = I2cBus::get_instance(bus_num);
    if (!bus) {
      auto msg = std::format("Failed to get I2C bus handle! Error: {}", bus.error().message());
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::runtime_error(msg);
    }

    auto sensor_res = Aht20Driver::create(bus.value(), measure_timeout, measure_wait_period);
    if (!sensor_res) {
      auto msg = std::format(
          "Failed to initialize temperature and humidity sensor! Error: {}",
          sensor_res.error().message());
      RCLCPP_FATAL(get_logger(), "%s", msg.c_str());
      throw std::runtime_error(msg);
    }
    sensor_ = std::move(sensor_res.value());
    RCLCPP_INFO(get_logger(), "Initialized temperature/humidity sensor!");

    auto period = std::chrono::duration<double>(1.0 / pub_rate_hz);
    timer_ = this->create_wall_timer(period, [this]() { timer_callback(); });
    RCLCPP_INFO(get_logger(), "Started temperature/humidity sensor node!");
  }

private:
  void timer_callback()
  {
    auto result = sensor_->read();
    if (!result) {
      RCLCPP_ERROR(
          get_logger(),
          "Error encountered while reading temperature and humidity sensor! Error: %s",
          result.error().message().c_str());
      return;
    }

    sensor_msgs::msg::Temperature temp_msg;
    temp_msg.header.stamp = this->now();
    temp_msg.header.frame_id = link_;
    temp_msg.variance = temp_var_;
    temp_msg.temperature = result.value().temperature;

    this->temp_publisher_->publish(temp_msg);

    sensor_msgs::msg::RelativeHumidity humid_msg;
    humid_msg.header.stamp = this->now();
    humid_msg.header.frame_id = link_;
    humid_msg.variance = humid_var_;
    humid_msg.relative_humidity = result.value().relative_humidity;

    this->humid_publisher_->publish(humid_msg);
  }

  double temp_var_;
  double humid_var_;
  std::string link_;

  static constexpr uint16_t kDefaultPubRateHz = 10;
  static constexpr uint16_t kDefaultMeasurementTimeout = 100;
  static constexpr uint16_t kDefaultPollPeriod = 5;

  // max errors from datasheet, assuming uniform distribution
  static constexpr double kDefaultTempVar = (2.0 * 2.0) / 3.0;
  static constexpr double kDefaultHumidVar = (0.05 * 0.05) / 3.0;

  std::unique_ptr<Aht20Driver> sensor_;

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<sensor_msgs::msg::Temperature>::SharedPtr temp_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::RelativeHumidity>::SharedPtr humid_publisher_;
};

} // namespace sensors

RCLCPP_COMPONENTS_REGISTER_NODE(sensors::TempHumidSensorNode);
