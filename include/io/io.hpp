
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

#define BOOST_ASIO_DISABLE_BOOST_COROUTINE

#include "base/elapsed.hpp"
#include "base/types.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/system.hpp>
#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using const_buff = asio::const_buffer;
using error_code = boost::system::error_code;
using io_context = asio::io_context;
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;
using ip_udp = boost::asio::ip::udp;
using steady_timer = asio::steady_timer;
using system_timer = asio::system_timer;
using strand = io_context::strand;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;
using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

static constexpr uint16_t ANY_PORT{0};

namespace io {

static constexpr error_code make_error(errc::errc_t val = errc::success) {
  return error_code(val, sys::generic_category());
}

const string is_ready(tcp_socket &sock, error_code ec = make_error(), bool cancel = true) noexcept;

const string log_socket_msg(error_code ec, tcp_socket &sock, const tcp_endpoint &r,
                            Elapsed e = Elapsed()) noexcept;

} // namespace io

} // namespace pierre