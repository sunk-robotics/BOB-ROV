#include <cstddef>
#include <limits>
#include <shared_mutex>
#include <system_error>
#include <vector>

#include "geometry_msgs/msg/accel.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "rcl_interfaces/msg/integer_range.hpp"
#include "rcl_interfaces/msg/parameter_descriptor.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/fluid_pressure.hpp"
#include "sensor_msgs/msg/relative_humidity.hpp"
#include "sensor_msgs/msg/temperature.hpp"

#include "comms/http_server.hpp"
#include "rtc/common.hpp"

namespace comms {

class WebRtcNode : public rclcpp::Node {
public:
  WebRtcNode(const rclcpp::NodeOptions &options)
      : Node("webrtc_node", options), http_srv_(make_http_server())
  {
    RCLCPP_INFO(get_logger(), "Setup HTTP server");

    rcl_interfaces::msg::ParameterDescriptor pub_rate_desc;
    rcl_interfaces::msg::IntegerRange pub_rate_range;
    pub_rate_range.from_value = 1;
    pub_rate_range.to_value = 200;
    pub_rate_range.step = 1;
    pub_rate_desc.integer_range = {pub_rate_range};
    this->declare_parameter("pub_rate_hz", kDefaultPubRateHz, pub_rate_desc);

    this->declare_parameter("pose_topic", "rov/pose");
    this->declare_parameter("twist_topic", "rov/twist");
    this->declare_parameter("accel_topic", "rov/accel");
    this->declare_parameter("pressure_topic", "pressure/pressure");
    this->declare_parameter("exterior_temp_topic", "pressure/temp");
    this->declare_parameter("interior_temp_topic", "temphumid/temp");
    this->declare_parameter("relative_humidity_topic", "temphumid/humid");

    const auto pub_rate_hz = this->get_parameter("pub_rate_hz").as_int();

    const auto pose_topic = this->get_parameter("pose_topic").as_string();
    const auto twist_topic = this->get_parameter("twist_topic").as_string();
    const auto accel_topic = this->get_parameter("accel_topic").as_string();
    const auto pressure_topic = this->get_parameter("pressure_topic").as_string();
    const auto exterior_temp_topic = this->get_parameter("exterior_temp_topic").as_string();
    const auto interior_temp_topic = this->get_parameter("interior_temp_topic").as_string();
    const auto humidity_topic = this->get_parameter("relative_humidity_topic").as_string();
    RCLCPP_INFO(get_logger(), "Loaded webrtc_node params");

    if (!http_srv_.start()) {
      static constexpr auto msg = "Failed to start HTTP server!";
      RCLCPP_FATAL(get_logger(), msg);
      throw std::runtime_error(msg);
    }
    RCLCPP_INFO(get_logger(), "Started HTTP server!");

    pose_sub_ = make_subscription<geometry_msgs::msg::Pose>(
        pose_topic, [](auto &frame, auto &msg) { frame.pose = msg; });
    twist_sub_ = make_subscription<geometry_msgs::msg::Twist>(
        twist_topic, [](auto &frame, auto &msg) { frame.twist = msg; });
    accel_sub_ = make_subscription<geometry_msgs::msg::Accel>(
        accel_topic, [](auto &frame, auto &msg) { frame.accel = msg; });
    pressure_sub_ = make_subscription<sensor_msgs::msg::FluidPressure>(
        pressure_topic, [](auto &frame, auto &msg) { frame.pressure = msg.fluid_pressure; });
    exterior_temp_sub_ = make_subscription<sensor_msgs::msg::Temperature>(
        exterior_temp_topic, [](auto &frame, auto &msg) { frame.exterior_temp = msg.temperature; });
    interior_temp_sub_ = make_subscription<sensor_msgs::msg::Temperature>(
        interior_temp_topic, [](auto &frame, auto &msg) { frame.interior_temp = msg.temperature; });
    humidity_sub_ = make_subscription<sensor_msgs::msg::RelativeHumidity>(
        humidity_topic,
        [](auto &frame, auto &msg) { frame.relative_humidity = msg.relative_humidity; });

    RCLCPP_INFO(get_logger(), "Created web_rtc_node callbacks");

    auto telemetry_period = std::chrono::duration<double>(1.0 / pub_rate_hz);
    telemetry_timer_ =
        this->create_wall_timer(telemetry_period, [this] { telemetry_timer_callback(); });
    RCLCPP_INFO(get_logger(), "Started web_rtc_node!");
  }

private:
  // small helper function to avoid repetition in the constructor
  template <typename MsgT, typename SetterT>
  rclcpp::Subscription<MsgT>::SharedPtr make_subscription(const std::string &topic, SetterT setter)
  {
    return this->create_subscription<MsgT>(topic, 1, [this, setter](MsgT::SharedPtr msg) {
      std::shared_lock lock(telem_mutex_, std::try_to_lock);
      if (!lock.owns_lock())
        return;
      setter(telem_frame_, *msg);
    });
  }

  struct TelemetryFrame {
    geometry_msgs::msg::Pose pose;
    geometry_msgs::msg::Twist twist;
    geometry_msgs::msg::Accel accel;
    double pressure;
    double exterior_temp;
    double interior_temp;
    double relative_humidity;
  };

  // we can't copy the server into a local variable so we do some weird magic in the initializer
  // list
  // i hate c++ ;-;
  [[nodiscard]] HttpServer make_http_server()
  {
    this->declare_parameter("webgui_path", "/webgui");

    rcl_interfaces::msg::ParameterDescriptor port_desc;
    rcl_interfaces::msg::IntegerRange port_range;
    port_range.from_value = 1024;
    port_range.to_value = 49151; // restrict to sensible port values
    port_range.step = 1;
    port_desc.integer_range = {port_range};
    this->declare_parameter("port", 8080, port_desc);

    rcl_interfaces::msg::ParameterDescriptor sdp_gathering_timeout_desc;
    rcl_interfaces::msg::IntegerRange sdp_gathering_timeout_range;
    sdp_gathering_timeout_range.from_value = 100;
    sdp_gathering_timeout_range.to_value = std::numeric_limits<uint16_t>::max();
    sdp_gathering_timeout_range.step = 1;
    sdp_gathering_timeout_desc.integer_range = {sdp_gathering_timeout_range};
    this->declare_parameter("sdp_gathering_timeout_ms", 5000, sdp_gathering_timeout_desc);

    const auto webgui_path = this->get_parameter("webgui_path").as_string();
    const auto port = this->get_parameter("port").as_int();
    const auto sdp_gathering_timeout = this->get_parameter("sdp_gathering_timeout_ms").as_int();
    RCLCPP_INFO(get_logger(), "Loaded HTTP server params");

    return HttpServer(
        webgui_path,
        port,
        sdp_gathering_timeout,
        [this]() { RCLCPP_INFO(get_logger(), "Connected to WebRTC remote!"); },
        [this]() {
          // just a warning; can happen occasionally with a weak connection
          RCLCPP_WARN(get_logger(), "WebRTC remote disconnected, attempting reconnection!");
        },
        [this](rtc::message_variant msg) {
          if (std::holds_alternative<rtc::binary>(msg))
            decode_unreliable_msg(std::get<rtc::binary>(msg));
          else
            handle_string_message();
        },
        [this](rtc::message_variant msg) {
          if (std::holds_alternative<rtc::binary>(msg))
            decode_reliable_msg(std::get<rtc::binary>(msg));
          else
            handle_string_message();
        },
        [this](std::error_code err) {
          RCLCPP_ERROR(
              get_logger(), "HTTP server encountered an error! Error: %s", err.message().c_str());
        });
  }

  inline void handle_string_message() noexcept
  {
    RCLCPP_ERROR(
        get_logger(), "Recieved string message over WebRTC. Check web client implementation!");
  }

  void decode_unreliable_msg(std::vector<std::byte> msg)
  {
    // implementation, talk to miles
  }

  void decode_reliable_msg(std::vector<std::byte> msg)
  {
    // implementation, talk to miles
  }

  void telemetry_timer_callback()
  {
    std::error_code ec;
    {
      std::lock_guard lock(telem_mutex_);

      ec = http_srv_.send_unreliable(
          reinterpret_cast<const std::byte *>(&telem_frame_), sizeof(TelemetryFrame));
    }

    if (ec) {
      if (ec == HttpServerErrorCode::channel_not_ready)
        RCLCPP_WARN(get_logger(), "%s", ec.message().c_str());
      else if (ec == HttpServerErrorCode::channel_send_failed) {
        RCLCPP_ERROR(get_logger(), "%s", ec.message().c_str());
      }
    }
  }

  static constexpr uint16_t kDefaultPubRateHz = 50;

  HttpServer http_srv_;
  std::shared_mutex telem_mutex_;
  TelemetryFrame telem_frame_;

  rclcpp::TimerBase::SharedPtr telemetry_timer_;

  rclcpp::Subscription<geometry_msgs::msg::Pose>::SharedPtr pose_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr twist_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Accel>::SharedPtr accel_sub_;
  rclcpp::Subscription<sensor_msgs::msg::FluidPressure>::SharedPtr pressure_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Temperature>::SharedPtr exterior_temp_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Temperature>::SharedPtr interior_temp_sub_;
  rclcpp::Subscription<sensor_msgs::msg::RelativeHumidity>::SharedPtr humidity_sub_;
};
} // namespace comms

RCLCPP_COMPONENTS_REGISTER_NODE(comms::WebRtcNode);
