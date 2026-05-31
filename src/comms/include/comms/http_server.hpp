#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <thread>

#include "httplib.h"
#include "rtc/rtc.hpp"

enum class HttpServerErrorCode : uint8_t {
  peer_connection_failed = 1,
  no_local_description,
  ice_gathering_time_out,
  channel_not_ready,
  channel_send_failed,
  stop_failed_in_deconstructor,
  multiple_complete_ice_gathering
};

struct HttpServerErrorCategory : std::error_category {
  [[nodiscard]] const char *name() const noexcept override { return "HttpServer"; }

  [[nodiscard]] std::string message(int ev) const override
  {
    switch (static_cast<HttpServerErrorCode>(ev)) {
      case HttpServerErrorCode::peer_connection_failed:
        return "Peer connection state updated to Failed, attempting reconnection!";
      case HttpServerErrorCode::no_local_description:
        return "No local SPD description available despite waiting for ICE!";
      case HttpServerErrorCode::ice_gathering_time_out:
        return "ICE gathering exceeded provided timeout period! Attempting reconnection!";
      case HttpServerErrorCode::channel_not_ready:
        return "Channel is null. Must be before construction or mid-reconnect!";
      case HttpServerErrorCode::channel_send_failed: return "Failed to send message over channel!";
      case HttpServerErrorCode::stop_failed_in_deconstructor:
        return "Failed to close channel in class deconstructor!";
      case HttpServerErrorCode::multiple_complete_ice_gathering:
        return "Returned complete status multiple times from ICE gathering!";
      default: return "Unknown HttpServerError";
    }
  }
};

[[nodiscard]] inline const HttpServerErrorCategory &http_server_error_category()
{
  static HttpServerErrorCategory instance;
  return instance;
}

[[nodiscard]] inline std::error_code make_error_code(HttpServerErrorCode e) noexcept
{ return {static_cast<int>(e), http_server_error_category()}; }

namespace std {
template <> struct is_error_code_enum<HttpServerErrorCode> : true_type {};
} // namespace std

class HttpServer {
public:
  // Taken from
  // https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState

  // A usable pairing of local and remote candidates has been found for all components of the
  // connection, and the connection has been established. It is possible that gathering is still
  // underway, and it is also possible that the ICE agent is still checking candidates against one
  // another looking for a better connection to use.
  using ConnectedCb = std::function<void()>;

  // Checks to ensure that components are still connected failed for at least one component of the
  // RTCPeerConnection. This is a less stringent test than failed and may trigger intermittently and
  // resolve just as spontaneously on less reliable networks, or during temporary disconnections.
  // When the problem resolves, the connection may return to the connected state.
  using DisconnectedCb = std::function<void()>;

  using UnreliableMessageCb = std::function<void(rtc::message_variant)>;
  using ReliableMessageCb = std::function<void(rtc::message_variant)>;

  using ErrorCb = std::function<void(std::error_code)>;

  HttpServer(
      const std::string &webgui_path,
      uint16_t port,
      uint16_t sdp_gathering_timeout_ms,
      ConnectedCb on_connected,
      DisconnectedCb on_disconnected,
      UnreliableMessageCb on_unreliable_msg,
      ReliableMessageCb on_reliable_msg,
      ErrorCb on_err)
      : port_(port), sdp_gathering_timeout_(sdp_gathering_timeout_ms), on_connected_(on_connected),
        on_disconnected_(on_disconnected), on_unreliable_msg_(on_unreliable_msg),
        on_reliable_msg_(on_reliable_msg), on_err_(on_err)
  {
    srv_.new_task_queue = [] { return new httplib::ThreadPool(1); };
    srv_.set_mount_point("/", webgui_path);

    srv_.Post("/offer", [this](auto &req, auto &res) { handle_post_offer(req, res); });
    srv_.Post("/disconnect", [this](auto &req, auto &res) { handle_post_disconnect(req, res); });
  }

  [[nodiscard]] bool start()
  {
    std::promise<bool> ready;
    auto future = ready.get_future();

    srv_thread_ = std::thread([this, &ready] { ready.set_value(srv_.listen("0.0.0.0", port_)); });

    // block until we know the server started correctly
    bool ok = future.get();
    if (!ok) {
      srv_thread_.join();
      return false;
    }

    // A dedicated reconnect thread is necessary because create_peer_connection() and
    // close_peer_connection() modify the state of the peer connection on a thread controlled by
    // that same peer connection. This could potentially be unsafe because of whatever internal
    // locks the library holds.
    //
    // A detached thread is not used because if the connection repeatedly fails, each failure would
    // spawn a new detached thread, creating an unbounded number of threads the dedicated thread is
    // less resource intensive and is easier to clean up in stop().
    reconnect_thread_ = std::thread([this] {
      while (true) {
        std::unique_lock lock(reconnect_mutex_);
        reconnect_cv_.wait(lock, [this] { return reconnect_requested_ || reconnect_stop_; });

        // reconnect stop is just a flag to wake up the thread so it can be joined
        if (reconnect_stop_)
          return;

        reconnect_requested_ = false;
        lock.unlock();

        close_peer_connection();
        create_peer_connection();
        on_connected_();
      }
    });

    return true;
  };

  [[nodiscard]] bool stop()
  {
    reconnect_stop_ = true;
    reconnect_cv_.notify_one();
    reconnect_thread_.join();

    close_peer_connection();

    srv_.stop();
    if (srv_thread_.joinable()) {
      srv_thread_.join();
      return true;
    }

    return false;
  }

  [[nodiscard]] std::error_code send_unreliable(const std::byte *data, size_t size)
  {
    std::lock_guard lock(unreliable_ch_mutex_);

    if (!unreliable_ch_)
      return HttpServerErrorCode::channel_not_ready;

    if (!unreliable_ch_->send(data, size))
      return HttpServerErrorCode::channel_send_failed;

    return {};
  }

  [[nodiscard]] std::error_code send_reliable(const std::byte *data, size_t size)
  {
    std::lock_guard lock(reliable_ch_mutex_);

    if (!reliable_ch_)
      return HttpServerErrorCode::channel_not_ready;

    if (!reliable_ch_->send(data, size))
      return HttpServerErrorCode::channel_send_failed;

    return {};
  }

  ~HttpServer()
  {
    if (!stop())
      on_err_(HttpServerErrorCode::stop_failed_in_deconstructor);
  }

private:
  void create_peer_connection()
  {

    rtc::Configuration config;
    auto pc = std::make_shared<rtc::PeerConnection>(config);

    pc->onStateChange([this](rtc::PeerConnection::State state) {
      switch (state) {
        case rtc::PeerConnection::State::Connected: on_connected_(); break;
        case rtc::PeerConnection::State::Disconnected: on_disconnected_(); break;
        case rtc::PeerConnection::State::Failed: {
          on_err_(HttpServerErrorCode::peer_connection_failed);
          {
            std::lock_guard lock(reconnect_mutex_);
            reconnect_requested_ = true;
          }
          reconnect_cv_.notify_one();
          break;
        }
        case rtc::PeerConnection::State::Connecting:
        case rtc::PeerConnection::State::Closed:
        case rtc::PeerConnection::State::New: break;
      }
    });

    pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> dc) {
      if (dc->label() == "unreliable") {
        std::lock_guard lock(unreliable_ch_mutex_);
        unreliable_ch_ = dc;
        unreliable_ch_->onMessage(on_unreliable_msg_);
      } else if (dc->label() == "reliable") {
        std::lock_guard lock(reliable_ch_mutex_);
        reliable_ch_ = dc;
        reliable_ch_->onMessage(on_reliable_msg_);
      }
    });

    std::lock_guard lock(pc_mutex_);
    pc_ = pc;
  };

  void close_peer_connection()
  {

    // lock channels here to prevent send_reliable/unreliable() call while calling pc_close() but
    // before the pointers are set to null;
    std::lock_guard lock_unreliable(unreliable_ch_mutex_);
    std::lock_guard lock_reliable(reliable_ch_mutex_);
    {
      std::lock_guard lock(pc_mutex_);
      pc_->close();
    }

    // clear stale channel pointer data in case of reconnection
    unreliable_ch_ = nullptr;
    reliable_ch_ = nullptr;
  }

  struct GatheringState {
    std::promise<void> complete;
    std::atomic<bool> fired{false};
  };

  void handle_post_offer(const httplib::Request &req, httplib::Response &res)
  {
    // use shared ptr to avoid race condition
    // without shared pointer in timeout path if the callback still fires after the timeout it will
    // try to set value on gathering_state.complete and gathering_state.fired which were already
    // destroyed when the function returned

    auto gathering_state = std::make_shared<GatheringState>();
    auto future = gathering_state->complete.get_future();

    // Gather all ICE candidates before returning
    // (avoids complication of trickle ICE when not really needed)
    {
      std::lock_guard lock(pc_mutex_);
      pc_->onGatheringStateChange(
          [this, gathering_state](rtc::PeerConnection::GatheringState state) {
            if (state == rtc::PeerConnection::GatheringState::Complete) {
              // shouldn't happen but this thing is going 500 meters under the
              // ocean so i am going to check if it somehow completed twice to
              // avoid throwing an exception and crashing the communications with topside

              bool expected = false;
              if (gathering_state->fired.compare_exchange_strong(expected, true)) {
                gathering_state->complete.set_value();
              } else {
                on_err_(HttpServerErrorCode::multiple_complete_ice_gathering);
              }
            }
          });
      pc_->setRemoteDescription(rtc::Description(req.body, "offer"));
    }

    // wait for ICE candidates gathering to finish
    // or for timeout period provided in class constructor
    if (future.wait_for(std::chrono::milliseconds(sdp_gathering_timeout_)) ==
        std::future_status::timeout) {
      on_err_(HttpServerErrorCode::ice_gathering_time_out);
      res.status = 504;
      res.set_content("ICE gathering timed out", "text/plain");
      {
        std::lock_guard lock(reconnect_mutex_);
        reconnect_requested_ = true;
      }
      reconnect_cv_.notify_one();
      return;
    }

    // send response to client with gathered ice description..
    std::optional<std::string> desc_res;
    {
      std::lock_guard lock(pc_mutex_);
      desc_res = pc_->localDescription();
    }

    if (!desc_res) {
      // also shouldn't happen
      // (we did just wait till we got a local description but better safe than no ROV)
      on_err_(HttpServerErrorCode::no_local_description);
      res.status = 500;
      res.set_content("No local description available", "text/plain");
    } else {
      res.status = 200;
      res.set_content(std::string(desc_res.value()), "text/plain");
    }
  };

  void handle_post_disconnect([[maybe_unused]] const httplib::Request &req, httplib::Response &res)
  {
    // we need to set up a new peer connection in case of reconnection
    {
      std::lock_guard lock(reconnect_mutex_);
      reconnect_requested_ = true;
    }
    reconnect_cv_.notify_one();

    res.status = 200;
    res.set_content("Disconnected OK", "text/plain");
  };

  const uint16_t port_;

  const uint16_t sdp_gathering_timeout_;

  const ConnectedCb on_connected_;
  const DisconnectedCb on_disconnected_;
  const UnreliableMessageCb on_unreliable_msg_;
  const ReliableMessageCb on_reliable_msg_;
  const ErrorCb on_err_;

  httplib::Server srv_;
  std::thread srv_thread_;

  std::mutex reconnect_mutex_;
  std::thread reconnect_thread_;
  std::condition_variable reconnect_cv_;
  bool reconnect_requested_ = false;
  bool reconnect_stop_ = false;

  std::mutex unreliable_ch_mutex_;
  std::shared_ptr<rtc::DataChannel> unreliable_ch_;
  std::mutex reliable_ch_mutex_;
  std::shared_ptr<rtc::DataChannel> reliable_ch_;

  std::mutex pc_mutex_;
  std::shared_ptr<rtc::PeerConnection> pc_;
};
