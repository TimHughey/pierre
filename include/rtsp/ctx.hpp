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
#include "frame/clock_info.hpp"
#include "io/io.hpp"
#include "rtsp/aes.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"
#include "rtsp/sessions.hpp"

#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <optional>

namespace pierre {

class Desk;

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

class Ctx : public std::enable_shared_from_this<Ctx> {

public:
  Ctx(io_context &io_ctx, auto sock, auto sessions, MasterClock *master_clock, Desk *desk) noexcept
      : io_ctx(io_ctx),             //
        feedback_timer(io_ctx),     // detect absence of routine feedback messages
        sock(sock),                 //
        sessions(sessions),         //
        master_clock(master_clock), //
        desk(desk) {}

  auto ptr() noexcept { return shared_from_this(); }

  void close() noexcept;

  void feedback_msg() noexcept;

  /// @brief Primary entry point for a session via Ctx
  void run() noexcept { asio::post(io_ctx, std::bind(&Ctx::msg_loop, this)); }

  void peers(const Peers &peer_list) noexcept;

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

private:
  // order dependent
  io_context &io_ctx;
  steady_timer feedback_timer;

public:
  // order dependent
  std::shared_ptr<tcp_socket> sock;
  Sessions *sessions;
  MasterClock *master_clock;
  Desk *desk;

  // order independent
  Aes aes;

  std::optional<Request> request;
  std::optional<Reply> reply;

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

  // workers
  std::shared_ptr<Audio> audio_srv;
  std::shared_ptr<Control> control_srv;
  std::shared_ptr<Event> event_srv;

private:
  std::atomic_bool teardown_in_progress{false};

public:
  static constexpr csv module_id{"rtsp.ctx"};
};

} // namespace rtsp
} // namespace pierre