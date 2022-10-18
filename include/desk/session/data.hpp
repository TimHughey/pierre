/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "base/elapsed.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "frame/frame.hpp"

#include <array>
#include <future>
#include <memory>
#include <optional>
#include <utility>

namespace pierre {
namespace desk {

class Data {
public:
  Data(io_context &io_ctx)
      : io_ctx(io_ctx),                                        // use shared io_ctx
        acceptor(io_ctx, tcp_endpoint(ip_tcp::v4(), ANY_PORT)) // listen on any port
  {}

  Port accept() {
    _socket.emplace(io_ctx); // create the socket for new connection
    acceptor.async_accept(*_socket, [this](const error_code ec) {
      if (!ec) {
        INFO(module_id, "ACCEPTED", "handle={}\n", _socket->native_handle());
        _socket->set_option(ip_tcp::no_delay(true));
      } else {
        disconnect(ec);
      }
    });

    return acceptor.local_endpoint().port();
  }

  tcp_socket &socket() { return _socket.value(); }

  void shutdown() {
    asio::post(io_ctx, [this]() {
      [[maybe_unused]] error_code ec;

      if (_socket) {
        _socket->close(ec);
      }

      acceptor.close(ec);
    });
  }

private:
  void disconnect(const error_code ec) {
    INFO(module_id, "DISCONNECT", "handle={} reason={}\n", _socket->native_handle(), ec.message());

    acceptor.close();
    _socket.reset();
  }

  // misc debug
  void log_connected();

private:
  // order dependent
  io_context &io_ctx;
  tcp_acceptor acceptor;

  // order independent
  std::optional<tcp_socket> _socket;

  // misc debug
public:
  static constexpr csv module_id{"DESK_DATA"};
};

} // namespace desk
} // namespace pierre