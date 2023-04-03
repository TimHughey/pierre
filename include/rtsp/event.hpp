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
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <memory>
#include <optional>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using strand_tp = asio::strand<asio::thread_pool::executor_type>;
using ip_tcp = boost::asio::ip::tcp;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;

namespace rtsp {

class Event {

public:
  Event(strand_tp &&strand) noexcept
      : local_strand(std::move(strand)),                              //
        acceptor(local_strand, tcp_endpoint(ip_tcp::v4(), ANY_PORT)), //
        sock_sess(local_strand, ip_tcp::v4()) {
    async_accept();
  }

  Port port() noexcept { return acceptor.local_endpoint().port(); }

private:
  // async_loop is invoked to:
  //  1. schedule the initial async accept
  //  2. after accepting a connection to schedule the next async connect
  void async_accept() noexcept {

    acceptor.async_accept([this](const error_code &ec, tcp_socket peer) {
      if (ec || !peer.is_open()) return;

      // this will close any previously opened connection
      sock_sess = std::move(peer);
      session_async_loop();

      async_accept();
    });
  }

  void session_async_loop() noexcept {
    // similiar to the Control UDP socket, Event is not used by AP2 however
    // the TCP socket must remain connected

    if (!sock_sess.is_open()) return;

    auto raw = uint8v();
    auto buff = asio::dynamic_buffer(raw);

    // in the event data is received quietly discard it and keep reading
    // note: the unique_ptr is moved into the closure so it remains in scope
    //       until the lambda finishes
    asio::async_read(sock_sess, buff, //
                     asio::transfer_at_least(1),
                     asio::append(
                         [this](const error_code &ec, ssize_t bytes, [[maybe_unused]] auto raw) {
                           if (ec) return;

                           bytes_recv += bytes;
                           session_async_loop();
                         },
                         std::move(raw)));
  }

private:
  // order dependent
  strand_tp local_strand;
  tcp_acceptor acceptor;
  tcp_socket sock_sess; // socket for active session

public:
  ssize_t bytes_recv;

public:
  static constexpr csv module_id{"RTSP EVENT"};
};

} // namespace rtsp
} // namespace pierre
