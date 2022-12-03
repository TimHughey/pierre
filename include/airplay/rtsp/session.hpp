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

#include "airplay/aes_ctx.hpp"
#include "airplay/rtsp/ctx.hpp"
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
#include <optional>

namespace pierre {
namespace rtsp {

class Session : public std::enable_shared_from_this<Session> {
public:
  using Packet = uint8v;
  using Wire = uint8v;

private:
  // the magic number of 117 represents the minimum size RTSP message expected
  // ** plain-text only, not accounting for encryption **
  //
  // POST /feedback RTSP/1.0
  // CSeq: 15
  // DACP-ID: DF86B6D21A6C805F
  // Active-Remote: 1570223890
  // User-Agent: AirPlay/665.13.1

  static auto transfer_initial() noexcept { return asio::transfer_at_least(117); }

  static constexpr csv CRLF{"\r\n"};
  static constexpr csv CRLFx2{"\r\n\r\n"};

private:
  Session(io_context &io_ctx, tcp_socket sock) noexcept
      : io_ctx(io_ctx),               // shared io_ctx
        sock(std::move(sock)),        // socket for this session
        aes_ctx(Host().device_id()),  // create aes ctx
        rtsp_ctx(Ctx::create(io_ctx)) // create ctx for this session
  {}

  template <typename CompletionCondition>
  void async_read(CompletionCondition &&cond, Elapsed &&e = Elapsed()) noexcept {
    // notes:
    //  1. nothing within this function can be captured by the lamba
    //     because the scope of this function ends before
    //     the lamba executes
    //
    //  2. the async_read call will attach the lamba to the io_ctx
    //     then immediately return and then this function returns
    //
    //  3. we capture a shared_ptr (self) for access within the lamba,
    //     that shared_ptr is kept in scope while async_read is
    //     waiting for socket data and while the lamba executes
    //
    //  4. when called again from within the lamba the sequence of
    //     events repeats (this function returns) and the shared_ptr
    //     once again goes out of scope
    //
    //  5. the crucial point -- we must keep the use count
    //     of the session above zero until the session ends
    //     (e.g. error, natural completion, io_ctx is stopped)

    asio::async_read(
        sock,                       // read from this socket
        asio::dynamic_buffer(wire), // into wire buffer (could be encrypted)
        std::move(cond),            // completion condition (bytes to transfer)
        [s = ptr(), e = std::forward<Elapsed>(e)](error_code ec, ssize_t bytes) mutable {
          if (s->packet.empty()) e.reset(); // start timing once we have data

          const auto msg = io::is_ready(s->sock, ec);

          if (!msg.empty()) {

            INFO(module_id, "ASYNC_READ", "{}\n", msg);
            // will fall out of scope when this function returns
          } else if (bytes < 1) {

            INFO(module_id, "ASYNC_READ", "retry, bytes={}\n", bytes);
            s->async_read(asio::transfer_at_least(1), std::move(e));

          } else if (s->sock.available() > 0) {

            // read available bytes (if any)
            s->async_read(std::forward<Elapsed>(e));
          } else {

            // handoff for decipher, parsing and reply
            s->do_packet(std::forward<Elapsed>(e));
          }
        });

    // misc notes:
    // 1. the first return of this function traverses back to the Server that
    //    created the Session (in the same io_ctx)
    // 2. subsequent returns are to the io_ctx and match the required void return
    //    signature
  }

  // async_read:
  //
  // invokes async_read() with a specific number of bytes request based on
  // bytes available on the socket or, when zero, a single byte
  //
  void async_read(Elapsed &&e) noexcept {

    if (const auto avail = sock.available(); avail > 0) {
      async_read(asio::transfer_exactly(avail), std::forward<Elapsed>(e));
    } else {
      async_read(asio::transfer_at_least(1), std::forward<Elapsed>(e));
    }
  }

public:
  static auto create(io_context &io_ctx, tcp_socket &&sock) {
    // creates the shared_ptr and starts the async loop
    // the asyncLoop holds onto the shared_ptr until an error on the
    // socket is detected
    return std::shared_ptr<Session>(new Session(io_ctx, std::forward<tcp_socket>(sock)));
  }

  auto ptr() noexcept { return shared_from_this(); }

  void run(Elapsed accept_e) noexcept {
    const auto &r = sock.remote_endpoint();
    const auto msg = io::log_socket_msg("SESSION", io::make_error(), sock, r, accept_e);
    INFO(module_id, "RUN", "{}\n", msg);

    async_read(transfer_initial());
  }

  void teardown() noexcept {
    asio::post(io_ctx, [s = ptr()]() {
      [[maybe_unused]] error_code ec;
      s->sock.shutdown(tcp_socket::shutdown_both, ec);
      s->sock.close(ec);

      INFO(module_id, "TEARDOWN", "active_remote={} {}\n", s->rtsp_ctx->active_remote,
           ec.message());
    });
  }

private:
  void do_packet(Elapsed &&e) noexcept;

  static void save_packet(uint8v &packet) noexcept;

  // misc debug / logging
  // void dump(DumpKind dump_type = RawOnly);
  // void dump(const auto *data, size_t len) const;

private:
  // order dependent - initialized by constructor
  io_context &io_ctx; // shared io_ctx
  tcp_socket sock;
  AesCtx aes_ctx;
  std::shared_ptr<rtsp::Ctx> rtsp_ctx;

  Wire wire;     // socket data (maybe encrypted)
  Packet packet; // deciphered
  Headers headers;
  Content content;

  std::vector<size_t> _separators;

public:
  static constexpr csv module_id{"RTSP_SESSION"};
};

} // namespace rtsp
} // namespace pierre