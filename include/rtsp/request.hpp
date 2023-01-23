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

#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "lcs/logger.hpp"
#include "rtsp/headers.hpp"

#include <future>
#include <memory>

namespace pierre {
namespace rtsp {
// forward decls
class Ctx;

class Request : public std::enable_shared_from_this<Request> {

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

public:
  Request(std::shared_ptr<Ctx> ctx); // must be in .cpp to prevent header loop

  template <typename CompletionCondition> void async_read(CompletionCondition &&cond) {
    static constexpr csv fn_id{"async_read"};

    asio::async_read(
        sock, dynamic_buffer(), std::move(cond),
        [this, s = shared_from_this()](error_code ec, ssize_t bytes) { do_packet(ec, bytes); });
  }

  // template <typename CompletionCondition> void async_read(CompletionCondition &&cond) {
  //   static constexpr csv fn_id{"async_read"};
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

  // if (sock.is_open() == false) return;

  // asio::async_read(sock,             // read from this socket
  //                  dynamic_buffer(), // into wire buffer (could be encrypted)
  //                  std::move(cond),  // completion condition (bytes to transfer)
  //                  [this, s = shared_from_this()](error_code ec, ssize_t bytes) mutable {
  //                    if (packet.empty()) e.reset(); // start timing once we have data

  //                    const auto msg = io::is_ready(sock, ec);

  //                    if (!msg.empty()) {
  //                      // log message and fall out of scope
  //                      INFO(module_id, fn_id, "{} bytes={}\n", msg, bytes);
  //                      throw std::runtime_error(msg);
  //                    }

  //                    if (auto avail = sock.available(); avail > 0) {
  //                      // bytes available, read them
  //                      asio::read(sock, dynamic_buffer(), asio::transfer_exactly(avail), ec);

  //                      if (ec) return;
  //                    }

  //                    // handoff for decipher, parsing and reply
  //                    do_packet();
  //                  });

  // misc notes:
  // 1. this function returns void to match returning to the io_ctx or the
  //    original caller (on first invocation)
  // }

  /// @brief Perform an async_read of exactly the bytes available (or a minimum of one)
  ///        to continue collecting bytes that represent a complete packet
  /// @param e Elapsed time of overall async_read calls
  void async_read() {
    if (const auto avail = sock.available(); avail > 0) {

      async_read(asio::transfer_exactly(avail));
    } else {
      async_read(asio::transfer_at_least(1));
    }
  }

  auto buffered_bytes() const noexcept { return std::ssize(wire); }

  void do_packet(error_code ec, ssize_t bytes);

  auto dynamic_buffer() { return asio::dynamic_buffer(wire); }

  auto have_delims(const auto want_delims) const noexcept {
    return std::ssize(delims) == std::ssize(want_delims);
  }

  auto find_delims(const auto &&delims_want) noexcept {
    delims = packet.find_delims(delims_want);

    return std::ssize(delims) == std::ssize(delims_want);
  }

  std::shared_future<string> read_packet() {
    std::shared_future<string> fut = prom.get_future();

    async_read(transfer_initial());

    return fut;
  }

  void parse_headers();
  size_t populate_content() noexcept;

private:
  // order dependent
  std::shared_ptr<Ctx> ctx;
  tcp_socket &sock;

  // order independent
  std::promise<string> prom;

public:
  Headers headers;
  uint8v content;
  uint8v packet; // always dedciphered
  uint8v wire;   // maybe encrypted
  uint8v::delims_t delims;
  Elapsed e;

  static constexpr csv module_id{"rtsp::request"};
};

} // namespace rtsp
} // namespace pierre