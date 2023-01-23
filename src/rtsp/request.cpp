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

#include "request.hpp"
#include "aes.hpp"
#include "ctx.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"

#include <exception>
#include <span>
#include <utility>

namespace pierre {
namespace rtsp {

Request::Request(std::shared_ptr<Ctx> ctx) //
    : ctx(ctx->shared_from_this()),        //
      sock(ctx->sock)                      //
{}

void Request::do_packet(error_code ec, ssize_t bytes) {
  static constexpr csv fn_id{"do_packet"};

  if (packet.empty()) e.reset(); // start timing once we have data

  const auto msg = io::is_ready(sock, ec);

  if (!msg.empty()) {
    // log message and fall out of scope
    INFO(module_id, fn_id, "{} bytes={}\n", msg, bytes);
    prom.set_value(msg);
    return;
  }

  if (auto avail = sock.available(); avail > 0) {
    error_code ec;
    // bytes available, read them
    asio::read(sock, dynamic_buffer(), asio::transfer_exactly(avail), ec);

    if (ec) return;
  }

  // this function is invoked to:
  //  1. find the message delims
  //  2. parse the method / headers
  //  3. ensure all content is received
  //  4. create/send the reply

  if (const auto buffered = buffered_bytes(); buffered > 0) {
    auto aes = ctx->aes;
    if (auto consumed = aes->decrypt(wire, packet); consumed != buffered) {
      // incomplete decipher, read from socket
      async_read();
      return;
    }
  }

  // we potentially have a complete message, attempt to find the delimiters
  if (find_delims(std::array{CRLF, CRLFx2}) == false) {
    INFO(module_id, fn_id, "packet_size={} delims={}\n", std::ssize(packet), std::ssize(delims));

    // we need more data in the packet to continue
    async_read();
    return;
  }

  parse_headers();

  const auto more_bytes = populate_content();

  if (more_bytes > 0) {
    async_read(asio::transfer_exactly(more_bytes));
    return;
  }

  prom.set_value(string());
}

void Request::parse_headers() {
  if (headers.parse_ok == false) {
    // we haven't parsed headers yet, do it now
    auto parse_ok = headers.parse(packet, delims);

    if (parse_ok == false) throw(std::runtime_error(headers.parse_err));
  }
}

size_t Request::populate_content() noexcept {

  long more_bytes{0}; // assume we have all content

  if (headers.contains(hdr_type::ContentLength)) {
    const auto expected_len = headers.val<int64_t>(hdr_type::ContentLength);

    // validate we have a packet that contains all the content
    const auto content_begin = delims[1].first + delims[1].second;
    const auto content_avail = std::ssize(packet) - content_begin;

    // compare the content_end to the size of the packet
    if (content_avail == expected_len) {
      // packet contains all the content, create a span representing the content in the packet
      content.assign_span(std::span(packet.from_begin(content_begin), expected_len));
    } else {
      // packet does not contain all the content, read bytes from the socket
      more_bytes = expected_len - content_avail;
    }
  }

  return more_bytes;
}

} // namespace rtsp
} // namespace pierre