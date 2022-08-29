
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

#include "ArduinoJson.h"
#include "base/types.hpp"

#include <array>
#include <boost/asio.hpp>
#include <boost/system.hpp>
#include <cstdint>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using const_buff = asio::const_buffer;
using io_context = asio::io_context;
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;
using ip_udp = boost::asio::ip::udp;
using socket_base = boost::asio::socket_base;
using steady_timer = asio::steady_timer;
using strand = io_context::strand;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;
using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

static constexpr uint16_t ANY_PORT = 0;
typedef uint16_t Port;

// types of the various servers we will spin up
enum class ServerType : uint8_t { Audio, Event, Control, Rtsp };

namespace io {

static constexpr size_t DOC_DEFAULT_MAX_SIZE = 5 * 1024;
static constexpr size_t MSG_LEN_SIZE = sizeof(uint16_t);

typedef std::array<char, 2048> Packed;
using StaticDoc = StaticJsonDocument<DOC_DEFAULT_MAX_SIZE>;
using DynaDoc = DynamicJsonDocument;

static constexpr error_code make_error(auto val) {
  return error_code(val, sys::generic_category());
}
} // namespace io

} // namespace pierre