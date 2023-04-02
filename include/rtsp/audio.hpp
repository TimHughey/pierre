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
#include "rtsp/ctx.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
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
using steady_timer = asio::steady_timer;
using work_guard_tp = asio::executor_work_guard<asio::thread_pool::executor_type>;
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;

namespace rtsp {

class Audio : public std::enable_shared_from_this<Audio> {

private:
  Audio(auto &&strand, Ctx *ctx) noexcept
      : local_strand(std::move(strand)),                              //
        ctx(ctx),                                                     //
        acceptor{local_strand, tcp_endpoint{ip_tcp::v4(), ANY_PORT}}, //
        sock(local_strand)                                            //
  {}

public:
  ~Audio() noexcept {}

  Port port() noexcept { return acceptor.local_endpoint().port(); }
  std::shared_ptr<Audio> ptr() noexcept { return shared_from_this(); }

  static auto start(auto &&strand, Ctx *ctx) noexcept {
    auto self = std::shared_ptr<Audio>(new Audio(std::forward<decltype(strand)>(strand), ctx));

    self->async_accept();

    return self;
  }

  void teardown() noexcept {
    [[maybe_unused]] error_code ec;

    acceptor.close(ec);
    sock.close(ec);
  }

private:
  // asyncLoop is invoked to:
  //  1. schedule the initial async accept
  //  2. after accepting a connection to schedule the next async connect
  //
  // with this in mind we accept an error code that is checked before
  // starting the next async_accept.  when the error code is not specified
  // assume this is startup and all is well.
  void async_accept() noexcept;

  void async_read_packet() noexcept;

  static const string is_ready(tcp_socket &sock,
                               error_code ec = error_code(errc::success, sys::generic_category()),
                               bool cancel = true) noexcept;

private:
  // order dependent
  strand_tp local_strand;
  Ctx *ctx;
  tcp_acceptor acceptor;
  tcp_socket sock;

  // order independent
  tcp_endpoint endpoint;
  uint8v packet_len;
  uint8v packet;

  static constexpr csv module_id{"rtsp.audio"};
};

} // namespace rtsp
} // namespace pierre
