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

#include "base/typical.hpp"

#include <boost/asio.hpp>
#include <cstdint>
#include <unordered_map>

namespace pierre {
namespace airplay {

using io_context = boost::asio::io_context;
using error_code = boost::system::error_code;
using high_res_timer = boost::asio::high_resolution_timer;
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;
using ip_udp = boost::asio::ip::udp;
using io_strand = boost::asio::io_context::strand;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;

enum class ServerType : uint8_t { Audio, Event, Control, Rtsp };

typedef uint64_t ClockId;
typedef uint16_t Port;
typedef uint32_t SeqNum;

typedef std::unordered_map<ServerType, Port> PortMap;

enum airplay_t { ap_1, ap_2 };

// the following is used even when not built for AirPlay 2
enum airplay_stream_c {
  unspecified_stream_category = 0,
  ptp_stream,
  ntp_stream,
  remote_control_stream
}; // "c" for category

enum airplay_stream_t { realtime_stream, buffered_stream };

enum clock_status_t : uint8_t {
  clock_no_anchor_info = 0,
  clock_ok,
  clock_service_unavailable,
  clock_access_error,
  clock_data_unavailable,
  clock_no_master,
  clock_version_mismatch,
  clock_not_synchronised,
  clock_not_valid,
  clock_not_ready,
};

enum timing_t { ts_ntp, ts_ptp };

} // namespace airplay
} // namespace pierre
