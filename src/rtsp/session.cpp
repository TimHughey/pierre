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

#include "session.hpp"
#include "base/elapsed.hpp"
#include "lcs/config.hpp"
#include "lcs/stats.hpp"
#include "reply.hpp"
#include "request.hpp"
#include "saver.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <utility>

namespace pierre {
namespace rtsp {

Session::Session(io_context &io_ctx, tcp_socket sock) noexcept
    : io_ctx(io_ctx),        // shared io_ctx
      sock(std::move(sock)), // socket for this session
      ctx(new Ctx(io_ctx))   // create ctx for this session
{}

std::shared_ptr<Session> Session::create(io_context &io_ctx, tcp_socket &&sock) noexcept {

  // creates the shared_ptr and starts the async loop
  // the asyncLoop holds onto the shared_ptr until an error on the
  // socket is detected
  return std::shared_ptr<Session>(new Session(io_ctx, std::forward<tcp_socket>(sock)));
}

void Session::do_packet(Elapsed &&e) noexcept {
  static constexpr csv fn_id{"do_packet"};

  // this function is invoked to:
  //  1. find the message delims
  //  2. parse the method / headers
  //  3. ensure all content is received
  //  4. create/send the reply

  if (const auto buffered = request.buffered_bytes(); buffered > 0) {
    if (const ssize_t consumed = ctx->aes_ctx.decrypt(request); consumed != buffered) {
      // incomplete decipher, read from socket
      async_read_request(std::move(e));
      return;
    }
  }

  // now we have potentially a complete message, attempt to find the delimiters
  if (request.find_delims(std::array{CRLF, CRLFx2}) == false) {
    INFO(module_id, fn_id, "packet_size={} delims={}\n", std::ssize(request.packet),
         std::ssize(request.delims));

    // we need more data in the packet to continue
    async_read_request(std::move(e));
    return;
  }

  request.parse_headers();

  const auto more_bytes = request.populate_content();

  if (more_bytes > 0) {
    async_read_request(asio::transfer_exactly(more_bytes), std::move(e));
    return;
  }

  Stats::write(stats::RTSP_SESSION_RX_PACKET, std::ssize(request.packet));

  Saver().format_and_write(request); // noop when config enable=false

  // update rtsp_ctx
  ctx->update_from(request.headers);

  // create the reply to the request
  Reply reply;
  reply.build(request, ctx->ptr());

  if (reply.empty()) { // warn of empty reply
    INFO(module_id, fn_id, "empty reply method={} path={}\n", request.headers.method(),
         request.headers.path());

    request = Request();

    async_read_request(transfer_initial());
    return;
  }

  Saver().format_and_write(reply);

  // reply has content to send
  ctx->aes_ctx.encrypt(reply.packet()); // NOTE: noop until cipher exchange completed

  // get the buffer to send, we will move reply into async_write lambda
  auto reply_buffer = reply.buffer();

  asio::async_write(sock, //
                    reply_buffer,
                    [this, s = ptr(), e = std::move(e),
                     rp = std::move(reply)](error_code ec, ssize_t bytes) mutable {
                      // write stats
                      Stats::write(stats::RTSP_SESSION_MSG_ELAPSED, e.freeze());
                      Stats::write(stats::RTSP_SESSION_TX_REPLY, bytes);

                      const auto msg = io::is_ready(sock, ec);

                      if (!msg.empty()) {
                        INFO(module_id, fn_id, "async_write failed, {}\n", msg);
                      }

                      // message handling complete, reset buffers
                      request = Request();
                      async_read_request(transfer_initial());
                    });

  // continue processing messages
}

} // namespace rtsp
} // namespace pierre
