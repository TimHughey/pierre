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

#include "audio.hpp"
#include "base/types.hpp"
#include "desk/desk.hpp"
#include "frame/frame.hpp"
#include "frame/racked.hpp"
#include "io/log_socket.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"

#include <ranges>
#include <vector>

namespace pierre {
namespace rtsp {

static constexpr auto PACKET_LEN_BYTES = sizeof(uint16_t);

void Audio::async_accept() noexcept {
  static constexpr csv fn_id{"async_accept"};

  // since the socket is wrapped in the optional and async_read() wants the actual
  // socket we must deference or get the value of the optional
  acceptor.async_accept(sock, endpoint, [this, e = Elapsed()](const error_code ec) {
    const auto msg = io::log_socket_msg(ec, sock, endpoint, e);
    INFO(module_id, fn_id, "{}\n", msg);

    // if the connected was accepted start the "session", otherwise fall through
    if (!ec) async_read_packet();
  });
}

void Audio::async_read_packet() noexcept {
  static constexpr csv fn_id{"async_read"};

  packet_len.clear();

  // start by reading the packet length
  asio::async_read(                             // async read the length of the packet
      sock,                                     // read from socket
      asio::dynamic_buffer(packet_len),         // into this buffer
      asio::transfer_exactly(PACKET_LEN_BYTES), // fill the entire buffer
      asio::bind_executor(                      // serialize so only one read is active
          local_strand,                         // of the local stand
          [this](error_code ec, ssize_t bytes) {
            const auto msg = is_ready(sock, ec);

            if (!msg.empty() || (bytes < std::ssize(packet_len))) {
              INFO(module_id, fn_id, "bytes={} {}\n", bytes, msg);
              return;
            }

            packet.clear();

            uint16_t len{0};
            len += packet_len[0] << 8;
            len += packet_len[1];

            if (len > 2) len -= sizeof(len);

            auto s = shared_from_this();

            asio::async_read(                 //
                sock,                         //
                asio::dynamic_buffer(packet), //
                asio::transfer_exactly(len),
                asio::bind_executor(local_strand, [=, s = s](error_code ec, ssize_t bytes) {
                  const auto msg = is_ready(s->sock, ec);

                  if (!msg.empty() || (bytes != len)) {
                    INFO(module_id, fn_id, "bytes={} msg\n", bytes, msg);
                    return;
                  }

                  s->ctx->desk->handoff(std::move(s->packet), s->ctx->shared_key);

                  if (s->sock.is_open()) s->async_read_packet();
                }));
          }));
}

const string Audio::is_ready(tcp_socket &sock, error_code ec, bool cancel) noexcept {

  // errc::operation_canceled:
  // errc::resource_unavailable_try_again:
  // errc::no_such_file_or_directory:

  string msg;

  // only generate log string on error or socket closed
  if (ec || !sock.is_open()) {
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{}", sock.is_open() ? "[O]" : "[X]");
    if (ec != errc::success) fmt::format_to(w, " {}", ec.message());
  }

  if (msg.size() && cancel) {
    [[maybe_unused]] error_code ec;
    sock.shutdown(tcp_socket::shutdown_both, ec);
    sock.close(ec);
  }

  return msg;
}

} // namespace rtsp
} // namespace pierre
