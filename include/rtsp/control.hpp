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

#include "base/uint8v.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <memory>

namespace pierre {
namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using strand_tp = asio::strand<asio::thread_pool::executor_type>;
using steady_timer = asio::steady_timer;
using work_guard_tp = asio::executor_work_guard<asio::thread_pool::executor_type>;
using ip_address = boost::asio::ip::address;
using ip_udp = boost::asio::ip::udp;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;

namespace rtsp {

// back to namespace server

class Control : public std::enable_shared_from_this<Control> {

public:
  uint16_t port() noexcept { return socket.local_endpoint().port(); }

  void teardown() noexcept {
    [[maybe_unused]] error_code ec;
    socket.close(ec); // since this is udp we only close the socket
  }

  std::shared_ptr<Control> ptr() noexcept { return shared_from_this(); }

  static auto start(auto &&strand) noexcept {
    auto s = std::shared_ptr<Control>(new Control(std::forward<decltype(strand)>(strand)));

    s->async_loop();

    return s->ptr();
  }

public:
  // create the Control
  Control(strand_tp &&strand)
      : local_strand(std::move(strand)),                           // strand
        socket(local_strand, udp_endpoint(ip_udp::v4(), ANY_PORT)) // create socket and endpoint
  {}

  void async_loop(const error_code ec = error_code(errc::success,
                                                   sys::generic_category())) noexcept {
    if (!ec && socket.is_open()) { // no error and socket is good
      // for AP2 we only need this socket open and don't do anything with any
      // data that might be received.  so, create and capture a unique_ptr that
      // simply goes away if any data is received.
      auto raw = std::make_unique<uint8v>(1024);
      auto buff = asio::buffer(*raw); // get the buffer before moving the ptr

      socket.async_receive(buff, [s = ptr(), raw = std::move(raw)](
                                     const error_code ec, [[maybe_unused]] size_t rx_bytes) {
        s->async_loop(ec); // will detect errors and close socket
      });
    } else {
      teardown();
    }
  }

private:
  // order dependent
  strand_tp local_strand;
  udp_socket socket;

public:
  static constexpr csv module_id{"rtsp.control"};
};

} // namespace rtsp
} // namespace pierre
