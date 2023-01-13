
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

#include "base/elapsed.hpp"
#include "base/types.hpp"

#include <array>
#include <boost/asio.hpp>
#include <boost/system.hpp>
#include <cstdint>
#include <fmt/format.h>
#include <iterator>
#include <map>
#include <mutex>
#include <shared_mutex>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using const_buff = asio::const_buffer;
using deadline_timer = asio::deadline_timer;
using io_context = asio::io_context;
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;
using ip_udp = boost::asio::ip::udp;
using socket_base = boost::asio::socket_base;
using steady_timer = asio::steady_timer;
using steady_timer_ptr = std::shared_ptr<steady_timer>;
using strand = io_context::strand;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;
using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
using streambuf_it = boost::asio::buffers_iterator<boost::asio::streambuf::const_buffers_type>;

static constexpr uint16_t ANY_PORT{0};
using Port = uint16_t;

namespace io {

struct contexts {

  io_context &make(const string &key) noexcept {
    std::unique_lock lck(mtx, std::defer_lock);
    lck.lock();

    auto [it, inserted] = map.try_emplace(key);

    return it->second;
  }

  template <typename T> io_context &make() noexcept {
    std::unique_lock lck(mtx, std::defer_lock);
    lck.lock();

    auto [it, inserted] = map.try_emplace(T::module_id.data());

    return it->second;
  }

  std::map<string, io_context> map;
  std::shared_mutex mtx;
};

static constexpr error_code make_error(errc::errc_t val = errc::success) {
  return error_code(val, sys::generic_category());
}

const string is_ready(tcp_socket &sock, error_code ec = make_error(), bool cancel = true) noexcept;

const string log_socket_msg(error_code ec, tcp_socket &sock, const tcp_endpoint &r,
                            Elapsed e = Elapsed()) noexcept;

} // namespace io

} // namespace pierre