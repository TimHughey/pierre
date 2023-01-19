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
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "rtsp/aes.hpp"
#include "rtsp/ctx.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/request.hpp"

#include <memory>
#include <optional>

namespace pierre {
namespace rtsp {

class Session : public std::enable_shared_from_this<Session> {
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
  Session(io_context &io_ctx, tcp_socket sock) noexcept;

  template <typename CompletionCondition>
  void async_read_request(CompletionCondition &&cond, Elapsed &&e = Elapsed()) noexcept {
    static constexpr csv fn_id{"async_read"};
    // notes:
    //  1. nothing within this function can be captured by the lamba
    //     because the scope of this function ends before
    //     the lamba executes
    //
    //  2. the async_read_request call will queue the work to the
    //     socket (via it's io_context) then immediately return and
    //     subsequently this function returns
    //
    //  3. we capture a shared_ptr to ourself for access within the lamba,
    //     that shared_ptr is kept in scope while async_read_request is
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
        sock,                     // read from this socket
        request.dynamic_buffer(), // into wire buffer (could be encrypted)
        std::move(cond),          // completion condition (bytes to transfer)
        [this, s = ptr(), e = std::forward<Elapsed>(e)](error_code ec, ssize_t bytes) mutable {
          if (request.packet.empty()) e.reset(); // start timing once we have data

          const auto msg = io::is_ready(sock, ec);

          if (!msg.empty()) {
            // log message and fall out of scope
            INFO(module_id, fn_id, "{} bytes={}\n", msg, bytes);
            return;
          }

          if (auto avail = sock.available(); avail > 0) {
            // bytes available, read them
            asio::read(sock, request.dynamic_buffer(), asio::transfer_exactly(avail), ec);

            if (ec) return;
          }

          // handoff for decipher, parsing and reply
          do_packet(std::forward<Elapsed>(e));
        });

    // misc notes:
    // 1. this function returns void to match returning to the io_ctx or the
    //    original caller (on first invocation)
  }

  /// @brief Perform an async_read of exactly the bytes available (or a minimum of one)
  ///        to continue collecting bytes that represent a complete packet
  /// @param e Elapsed time of overall async_read calls
  void async_read_request(Elapsed &&e) noexcept {
    if (const auto avail = sock.available(); avail > 0) {
      async_read_request(asio::transfer_exactly(avail), std::forward<Elapsed>(e));
    } else {
      async_read_request(asio::transfer_at_least(1), std::forward<Elapsed>(e));
    }
  }

  auto ptr() noexcept { return shared_from_this(); }

public:
  static std::shared_ptr<Session> create(io_context &io_ctx, tcp_socket &&sock) noexcept;

  void run(Elapsed accept_e) noexcept {
    static constexpr csv cat{"run"};

    const auto &r = sock.remote_endpoint();
    const auto msg = io::log_socket_msg(io::make_error(), sock, r, accept_e);
    INFO(module_id, cat, "{}\n", msg);

    async_read_request(transfer_initial());
  }

  void shutdown() noexcept {
    error_code ec;
    sock.shutdown(tcp_socket::shutdown_both, ec);
    INFO(module_id, "shutdown", "active_remote={} {}\n", ctx->active_remote, ec.message());

    sock.close(ec);

    if (ctx.use_count() > 0) ctx->teardown();
  }

private:
  void do_packet(Elapsed &&e) noexcept;

private:
  // order dependent - initialized by constructor
  io_context &io_ctx; // shared io_ctx
  tcp_socket sock;
  std::shared_ptr<Ctx> ctx;

  // order independent
  Request request; // accummulation of headers and content

public:
  static constexpr csv module_id{"rtsp.session"};
};

} // namespace rtsp
} // namespace pierre