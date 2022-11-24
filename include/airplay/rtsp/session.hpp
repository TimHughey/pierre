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

#include "base/aes/ctx.hpp"
#include "base/content.hpp"
#include "base/elapsed.hpp"
#include "base/headers.hpp"
#include "base/host.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "reply/inject.hpp"

#include <memory>

namespace pierre {
namespace rtsp {

class Session : public std::enable_shared_from_this<Session> {
public:
  enum DumpKind { RawOnly, HeadersOnly, ContentOnly };

public:
  static auto create(io_context &io_ctx, tcp_socket &&sock) {
    // creates the shared_ptr and starts the async loop
    // the asyncLoop holds onto the shared_ptr until an error on the
    // socket is detected
    return std::shared_ptr<Session>(new Session(io_ctx, std::forward<tcp_socket>(sock)));
  }

  auto ptr() noexcept { return shared_from_this(); }

private:
  Session(io_context &io_ctx, tcp_socket sock) noexcept
      : io_ctx(io_ctx),             //
        sock(std::move(sock)),      //
        aes_ctx(Host().device_id()) // create aes ctx
  {}

public:
  void run(Elapsed accept_e) noexcept {
    const auto &r = sock.remote_endpoint();
    const auto msg = io::log_socket_msg("SESSION", io::make_error(), sock, r, accept_e);
    INFO(module_id, "RUN", "{}\n", msg);

    async_loop();
  }

  void teardown() noexcept {
    try {
      sock.shutdown(tcp_socket::shutdown_both);
      sock.close();
    } catch (...) {
    }
  }

private:
  void async_loop() noexcept; // see .cpp file for critical function details
  bool createAndSendReply();
  bool ensureAllContent(); // uses Headers to ensure all content is loaded

  bool is_ready(const error_code ec = io::make_error()) noexcept {
    auto rc = sock.is_open();

    if (rc) {
      switch (ec.value()) {
      case errc::success:
        break;

      case errc::operation_canceled:
      case errc::resource_unavailable_try_again:
      case errc::no_such_file_or_directory:
        rc = false;
        break;

      default:
        INFO(module_id, "NOT READY", "socket={} {}\n", sock.native_handle(), ec.message());
        teardown();

        rc = false;
      }
    }

    return rc;
  }

  bool rxAvailable(); // load bytes immediately available
  static void save_packet(uint8v &packet) noexcept;
  uint8v &wire() { return _wire; }

  // misc debug / logging
  void dump(DumpKind dump_type = RawOnly);
  void dump(const auto *data, size_t len) const;

private:
  // order dependent - initialized by constructor
  io_context &io_ctx;
  tcp_socket sock;
  AesCtx aes_ctx;

  uint8v _wire;   // plain text or ciphered
  uint8v _packet; // deciphered
  Headers _headers;
  Content _content;

  string active_remote;

public:
  static constexpr csv module_id{"RTSP_SESSION"};
};

} // namespace rtsp
} // namespace pierre