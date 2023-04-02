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
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <memory>
#include <optional>

namespace pierre {

using error_code = boost::system::error_code;
using strand_tp = asio::strand<asio::thread_pool::executor_type>;
using steady_timer = asio::steady_timer;
using work_guard_tp = asio::executor_work_guard<asio::thread_pool::executor_type>;
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;

namespace rtsp {

class Event : public std::enable_shared_from_this<Event> {
private:
  Event(strand_tp &&strand)
      : local_strand(std::move(strand)),                             //
        acceptor{local_strand, tcp_endpoint(ip_tcp::v4(), ANY_PORT)} //
  {}

public:
  Port port() noexcept { return acceptor.local_endpoint().port(); }
  void teardown() noexcept {

    [[maybe_unused]] error_code ec;
    acceptor.close(ec);

    if (sock_accept.has_value()) sock_accept->close(ec);
    if (sock_session.has_value()) sock_session->close(ec);
  }

  std::shared_ptr<Event> ptr() noexcept { return shared_from_this(); }

  static auto start(auto &&strand) noexcept {
    auto self = std::shared_ptr<Event>(new Event(std::forward<decltype(strand)>(strand)));

    self->async_accept();

    return self;
  }

private:
  // async_loop is invoked to:
  //  1. schedule the initial async accept
  //  2. after accepting a connection to schedule the next async connect
  //
  // with this in mind we accept an error code that is checked before
  // starting the next async_accept.  when the error code is not specified
  // assume this is startup and all is well.
  void async_accept(const error_code ec = io::make_error(errc::success)) noexcept {

    if (!ec && acceptor.is_open()) {

      // this is the socket for the next accepted connection, store it in an optional
      // for use within the lambda
      sock_accept.emplace(local_strand);

      // since the io_ctx is wrapped in the optional and async_loop wants the actual
      // io_ctx we must deference or get the value of the optional
      acceptor.async_accept(*sock_accept, [s = ptr()](error_code ec) {
        if (!ec && s->sock_accept.has_value()) {

          // if there was a previous session, clear it
          s->sock_session.reset();
          s->sock_accept.swap(s->sock_session); // swap the accepted socket into the session socket

          // wait for session data
          s->session_async_loop(ec);

          // listen for the next connection request
          s->async_accept(ec);
        } else {
          s->teardown();
        }
      });
    } else {
      teardown();
    }
  }

  void session_async_loop(const error_code ec) noexcept {
    // similiar to the Control UDP socket, Event is not used by AP2 however
    // the TCP socket must remain connected

    if (!ec && sock_session.has_value()) {
      auto raw = std::make_unique<uint8v>();
      auto buff = asio::dynamic_buffer(*raw);

      // in the event data is received quietly discard it and keep reading
      // note: the unique_ptr is moved into the closure so it remains in scope
      //       until the lambda finishes
      asio::async_read(*sock_session, buff, //
                       asio::transfer_at_least(1),
                       [raw = std::move(raw), s = ptr()](error_code ec, ssize_t bytes) {
                         s->bytes_recv += bytes;

                         s->session_async_loop(ec); // recurse (which checks for errors)
                       });
    } else {
      teardown(); // and fall through
    }
  }

private:
  // order dependent
  strand_tp local_strand;
  tcp_acceptor acceptor;

  std::optional<tcp_socket> sock_accept;  // socket for next accepted connection
  std::optional<tcp_socket> sock_session; // socket for active session

public:
  ssize_t bytes_recv;

public:
  static constexpr csv module_id{"RTSP EVENT"};
};

} // namespace rtsp
} // namespace pierre
