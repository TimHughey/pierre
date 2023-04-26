//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "rtsp/aes.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"

#include <atomic>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using steady_timer = asio::steady_timer;
using ip_address = asio::ip::address;
using tcp_endpoint = asio::ip::tcp::endpoint;
using tcp_socket = asio::ip::tcp::socket;

class Desk;
class Rtsp;

namespace rtsp {
// forward decls to prevent excessive header includes
class Aes;
class Audio;
class Control;
class Event;

enum ports_t { AudioPort, ControlPort, EventPort };

struct stream_info_t {
  enum class cat : uint64_t { unspecified = 10, ptp_stream, ntp_stream, remote_control };
  enum class type : uint64_t { none = 0, realtime = 96, buffered = 103 };
  enum class proto : uint64_t { none = 0, ntp, ptp };

  constexpr bool is_ntp_stream() noexcept { return timing_cat == cat::ntp_stream; }
  constexpr bool is_ptp_stream() noexcept { return timing_cat == cat::ptp_stream; }
  constexpr bool is_remote_control() noexcept { return timing_cat == cat::remote_control; }
  constexpr bool is_buffered() noexcept { return timing_type == type::buffered; }
  constexpr bool is_realtime() noexcept { return timing_type == type::realtime; }

  bool supports_dynamic_stream_id{false};
  string client_id;
  uint64_t audio_format{0};
  uint64_t audio_mode{0};
  uint64_t conn_id{0}; // stream connection id
  uint64_t ct{0};      // compression type
  uint64_t spf{0};     // sample frames per packet

  // timing
  cat timing_cat{cat::unspecified};
  type timing_type{type::none};
  proto timing_proto{proto::none};
};

class Ctx {

public:
  Ctx(tcp_socket &&peer, Rtsp *rtsp, Desk *desk) noexcept;

  ~Ctx() noexcept;

  static auto create(auto &&peer, Rtsp *rtsp, Desk *desk) noexcept {
    return std::make_unique<Ctx>(std::forward<decltype(peer)>(peer), rtsp, desk);
  }

  void feedback_msg() noexcept;

  void force_close() noexcept;

  Port server_port(ports_t server_type) noexcept;

  void set_live() noexcept;

  void setup_stream(const auto timing_protocol) noexcept {
    if (timing_protocol == csv{"PTP"}) {
      stream_info.timing_cat = stream_info_t::cat::ptp_stream;
      stream_info.timing_proto = stream_info_t::proto::ptp;

    } else if (timing_protocol == csv{"NTP"}) {
      stream_info.timing_cat = stream_info_t::cat::ptp_stream;
      stream_info.timing_proto = stream_info_t::proto::ntp;

    } else {
      stream_info.timing_cat = stream_info_t::cat::remote_control;
      stream_info.timing_proto = stream_info_t::proto::none;
    }
  }

  template <typename T> T setup_stream_type(T type) noexcept {
    if (type == static_cast<T>(stream_info_t::type::buffered)) {
      stream_info.timing_type = stream_info_t::type::buffered;
    } else if (type == static_cast<T>(stream_info_t::type::realtime)) {
      stream_info.timing_type = stream_info_t::type::realtime;
    } else {
      stream_info.timing_type = stream_info_t::type::none;
    }

    return type;
  }

  void teardown() noexcept;

private:
  /// @brief Primary loop for RTSP message handling
  void msg_loop() noexcept;
  void msg_loop_read() noexcept;
  void msg_loop_write() noexcept;

public:
  // order dependent
  asio::io_context io_ctx;
  tcp_socket sock;
  Rtsp *rtsp{nullptr};
  Desk *desk{nullptr};
  Aes aes;

private:
  // order dependent (private)
  steady_timer feedback_timer;
  std::atomic_bool teardown_in_progress;
  std::jthread thread;

public:
  // order independent
  std::optional<Request> request;
  std::optional<Reply> reply;
  std::atomic_bool live{false};

public:
  // from RTSP headers
  int64_t cseq{0};          // i.e. CSeq: 8 (active session seq num, increasing from zero)
  int64_t active_remote{0}; // i.e. Active-Remote: 1570223890
  int64_t proto_ver{0};     // i.e. X-Apple-ProtocolVersion: 1
  string client_name;       // i.e. X-Apple-Client-Name: xapham
  string dacp_id;           // i.e. DACP-ID: DF86B6D21A6C805F
  string user_agent;        // i.e. User-Agent: AirPlay/665.13.1

  // from SETUP message
  bool group_contains_group_leader{false};
  uint8v shared_key; // shared key (for decipher)
  stream_info_t stream_info;
  string group_id; // airplay group id

private:
  // workers
  std::unique_ptr<Audio> audio_srv;
  std::unique_ptr<Control> control_srv;
  std::unique_ptr<Event> event_srv;

public:
  static constexpr csv module_id{"rtsp.ctx"};
  static constexpr csv thread_name{"rtsp_ctx"};
};

} // namespace rtsp
} // namespace pierre

/// @brief Custom formatter for Reel
template <> struct fmt::formatter<pierre::rtsp::Ctx> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::rtsp::Ctx &rtsp_ctx, FormatContext &ctx) const {

    const auto msg = fmt::format("remote={} dacp={} '{}'",
                                 rtsp_ctx.active_remote, //
                                 rtsp_ctx.dacp_id,       //
                                 rtsp_ctx.client_name);

    return formatter<std::string>::format(msg, ctx);
  }
};