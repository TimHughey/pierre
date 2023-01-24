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

  std::shared_future<string> read_packet() {
    std::shared_future<string> fut(prom.get_future());

    async_read(transfer_initial());

    return fut;
  }

private: // early decls for auto
  void do_packet(error_code ec, ssize_t bytes);

  auto have_delims(const auto want_delims) const noexcept {
    return std::ssize(delims) == std::ssize(want_delims);
  }

  auto find_delims(const auto &&delims_want) noexcept {
    delims = packet.find_delims(delims_want);

    return std::ssize(delims) == std::ssize(delims_want);
  }

private:
  template <typename CompletionCondition> void async_read(CompletionCondition &&cond) {
    static constexpr csv fn_id{"async_read"};

    asio::async_read(
        sock, asio::dynamic_buffer(wire), std::move(cond),
        // note: capturing this and shared_from_this() allows direct calls to
        // member functions while also ensuring the object remains in scope
        [this, s = shared_from_this()](error_code ec, ssize_t bytes) { do_packet(ec, bytes); });
  }

  /// @brief Re-entry point for async_read when more data from socket is required
  ///        This function will at read at least one byte or the exact number of
  //         bytes available on the socket
  void async_read() {
    if (const auto avail = sock.available(); avail > 0) {
      async_read(asio::transfer_exactly(avail));
    } else {
      async_read(asio::transfer_at_least(1));
    }
  }

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