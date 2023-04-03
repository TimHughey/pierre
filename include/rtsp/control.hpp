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

#include <boost/asio/append.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/udp.hpp>
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
using ip_address = boost::asio::ip::address;
using ip_udp = boost::asio::ip::udp;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;

namespace rtsp {

// back to namespace server

class Control {

public:
  // create the Control
  Control(auto &&strand)
      : local_strand(std::move(strand)),                         // strand
        sock(local_strand, udp_endpoint(ip_udp::v4(), ANY_PORT)) // create socket and endpoint
  {
    async_loop();
  }

public:
  uint16_t port() noexcept { return sock.local_endpoint().port(); }

  void async_loop() noexcept {
    if (!sock.is_open()) return;

    auto raw = uint8v(256);
    auto buff = asio::buffer(raw); // get the buffer before moving the ptr

    sock.async_receive(buff, //
                       asio::append(
                           [this](const error_code &ec, ssize_t, uint8v) {
                             if (ec) return;

                             async_loop();
                           },
                           std::move(raw)));
  }

private:
  // order dependent
  strand_tp local_strand;
  udp_socket sock;

public:
  static constexpr csv module_id{"rtsp.control"};
};

} // namespace rtsp
} // namespace pierre
