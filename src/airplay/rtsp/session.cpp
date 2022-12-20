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

#include "rtsp/session.hpp"
#include "base/elapsed.hpp"
#include "config/config.hpp"
#include "content.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"
#include "stats/stats.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <utility>

namespace pierre {
namespace rtsp {

// STATIC!

void Session::do_packet(Elapsed &&e) noexcept {

  // this function is invoked to:
  //  1. find the message delims
  //  2. parse the method / headers
  //  3. ensure all content is received
  //  4. create/send the reply

  if (const auto buffered = std::ssize(wire); buffered > 0) {
    const ssize_t consumed = ctx->aes_ctx.decrypt(packet, wire);

    // incomplete decipher, read from socket
    if (consumed != buffered) {
      INFO(module_id, "DO_PACKET", "incomplete, buffered={} consumed={} wire={} packet={}\n", //
           buffered, consumed, std::ssize(wire), std::ssize(packet));

      async_read_request(std::move(e));
      return;
    }
  }

  // now we have potentially a complete message, attempt to find the delimiters
  uint8v::delims_t found_delims{packet.find_delims(std::array{CRLF, CRLFx2})};

  if (const auto count = std::ssize(found_delims); count != 2) {
    INFO(module_id, "DELIMS", "packet_size={} delims={}\n", std::ssize(packet), count);

    // we need more data in the packet to continue
    async_read_request(std::move(e));
    return;
  }

  Stats::write(stats::RTSP_SESSION_RX_PACKET, std::ssize(packet));

  if (request.headers.parse_ok == false) {
    // we haven't parsed headers yet, do it now
    auto parse_ok = request.headers.parse(packet, found_delims);

    if (parse_ok == false) {
      INFO(module_id, "DO_PACKET", "FAILED parsing headers\n");
      return;
    }
  }

  // we now have complete headers, if there is content and we've not copied
  // it yet, attempt to copy it now or determine the packet is incomplete
  if (request.headers.contains(hdr_type::ContentLength)) {
    const auto content_expected_len = request.headers.val<int64_t>(hdr_type::ContentLength);

    // now, let's validate we have a packet that contains all the content
    const auto content_begin = found_delims[1].first + found_delims[1].second;
    const auto content_avail = std::ssize(packet) - content_begin;

    // compare the content_end to the size of the packet
    if (content_avail == content_expected_len) {
      // packet contains all the content, create a span representing the content in the packet
      request.content.assign_span(
          std::span(packet.from_begin(content_begin), content_expected_len));
    } else {
      // packet does not contain all the content, read bytes from the socket
      const auto more_bytes = content_expected_len - content_avail;

      async_read_request(asio::transfer_exactly(more_bytes), std::move(e));
      return;
    }
  }

  // ok, we now have the headers and content (if applicable)
  // create and send the reply

  request.save(packet); // noop when config enable=false

  // update rtsp_ctx
  ctx->update_from(request.headers);

  // create the reply to the request
  Reply reply;
  reply.build(request, ctx->ptr());

  if (reply.empty()) { // warn of empty reply
    INFO(module_id, "SEND REPLY", "empty reply method={} path={}\n", request.headers.method(),
         request.headers.path());

    async_read_request(transfer_initial());
    return;
  }

  // reply has content to send
  ctx->aes_ctx.encrypt(reply.packet()); // NOTE: noop until cipher exchange completed

  // get the buffer to send, we will move reply into async_write lambda
  auto reply_buffer = reply.buffer();

  asio::async_write(
      sock, reply_buffer,
      [s = ptr(), e = std::move(e), rp = std::move(reply)](error_code ec, ssize_t bytes) mutable {
        const auto msg = io::is_ready(s->sock, ec);

        if (!msg.empty()) {
          INFO(module_id, "WRITE_REPLY", "{}\n", msg);
        } else {

          Stats::write(stats::RTSP_SESSION_MSG_ELAPSED, e.freeze());
          Stats::write(stats::RTSP_SESSION_TX_REPLY, bytes);

          // message handling complete, reset buffers
          s->wire = uint8v();
          s->packet = uint8v();
          s->request = Request();

          // continue processing messages
          s->async_read_request(transfer_initial());
        }
      });
}

} // namespace rtsp
} // namespace pierre
