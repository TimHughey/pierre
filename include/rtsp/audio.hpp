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

#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
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
using ip_address = boost::asio::ip::address;
using ip_tcp = boost::asio::ip::tcp;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
using tcp_socket = boost::asio::ip::tcp::socket;

namespace rtsp {

class Audio {

public:
  Audio(auto &&strand, Ctx *ctx) noexcept
      : local_strand(std::move(strand)), ctx(ctx),
        streambuf(16 * 1024), // allow buffering of sixteen packets
        acceptor{local_strand, tcp_endpoint{ip_tcp::v4(), ANY_PORT}},
        sock(local_strand, ip_tcp::v4()) // overwritten by accepting socket
  {
    async_accept();
  }

  ~Audio() = default;

  Port port() noexcept { return acceptor.local_endpoint().port(); }

private:
  // asyncLoop is invoked to:
  //  1. schedule the initial async accept
  //  2. after accepting a connection to schedule the next async connect
  void async_accept() noexcept {

    acceptor.async_accept([this](const error_code &ec, tcp_socket peer) {
      // if the connected was accepted start the "session", otherwise fall through
      if (ec || !acceptor.is_open()) return;

      // closes previously opened socket
      sock = std::move(peer);

      async_read();
      async_accept();
    });
  }

  void async_read() noexcept;
  // void async_read_data(ssize_t data_len) noexcept;

private:
  // order dependent
  strand_tp local_strand;
  Ctx *ctx;
  asio::streambuf streambuf;
  tcp_acceptor acceptor;
  tcp_socket sock;

  // order independent
  std::ptrdiff_t packet_len{0};

  static constexpr csv module_id{"rtsp.audio"};
};

} // namespace rtsp
} // namespace pierre
