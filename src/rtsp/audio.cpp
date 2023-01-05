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
#include "frame/frame.hpp"
#include "frame/racked.hpp"
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
  acceptor.async_accept(sock, endpoint, [s = ptr(), e = Elapsed()](const error_code ec) {
    const auto msg = io::log_socket_msg(ec, s->sock, s->endpoint, e);
    INFO(module_id, fn_id, "{}\n", msg);

    // if the connected was accepted start the "session", otherwise fall through
    if (!ec) s->async_read_packet();
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
          [s = ptr()](error_code ec, ssize_t bytes) {
            auto &sock = s->sock;

            const auto msg = io::is_ready(sock, ec);

            if (!msg.empty() || (bytes < std::ssize(s->packet_len))) {
              INFO(module_id, fn_id, "bytes={} {}\n", bytes, msg);
              return;
            }

            s->packet.clear();

            uint16_t len{0};
            len += s->packet_len[0] << 8;
            len += s->packet_len[1];

            if (len > 2) len -= sizeof(len);

            asio::async_read(                    //
                sock,                            //
                asio::dynamic_buffer(s->packet), //
                asio::transfer_exactly(len),
                asio::bind_executor(s->local_strand, [=](error_code ec, ssize_t bytes) {
                  auto &sock = s->sock;

                  const auto msg = io::is_ready(sock, ec);

                  if (!msg.empty() || (bytes != len)) {
                    INFO(module_id, fn_id, "bytes={} msg\n", bytes, msg);
                    return;
                  }

                  Racked::handoff(std::move(s->packet), s->rtsp_ctx->shared_key);

                  s->async_read_packet();
                }));
          }));
}

} // namespace rtsp
} // namespace pierre
