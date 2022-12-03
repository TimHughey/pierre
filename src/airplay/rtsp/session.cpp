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
#include "reply/factory.hpp"
#include "reply/reply.hpp"
#include "stats/stats.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <span>
#include <utility>

namespace pierre {
namespace rtsp {

namespace fs = std::filesystem;

// STATIC!

void Session::do_packet(Elapsed &&e) noexcept {
  using namespace airplay::reply;

  // this function is invoked to:
  //  1. find the message delims
  //  2. parse the method / headers
  //  3. ensure all content is received
  //  4. create/send the reply

  if (const auto buffered = std::ssize(wire); buffered > 0) {
    const ssize_t consumed = aes_ctx.decrypt(packet, wire);

    // incomplete decipher, read from socket
    if (consumed != buffered) {
      INFO(module_id, "DO_PACKET", "incomplete, buffered={} consumed={} wire={} packet={}\n", //
           buffered, consumed, std::ssize(wire), std::ssize(packet));

      async_read(std::move(e));
      return;
    }
  }

  // now we have potentially a complete message, attempt to find the delimiters
  uint8v::delims_t found_delims{packet.find_delims(std::array{CRLF, CRLFx2})};

  if (const auto count = std::ssize(found_delims); count != 2) {
    INFO(module_id, "DELIMS", "packet_size={} delims={}\n", std::ssize(packet), count);

    // we need more data in the packet to continue
    async_read(std::move(e));
    return;
  }

  Stats::write(stats::RTSP_SESSION_RX_PACKET, std::ssize(packet));

  if (headers.parse_ok == false) {
    // we haven't parsed headers yet, do it now
    auto parse_ok = headers.parse(packet, found_delims);

    if (parse_ok == false) {
      INFO(module_id, "DO_PACKET", "FAILED parsing headers\n");
      return;
    }
  }

  // we now have complete headers, if there is content and we've not copied
  // it yet, attempt to copy it now or determine the packet is incomplete
  if (headers.contains(hdr_type::ContentLength)) {
    const auto content_expected_len = headers.val<int64_t>(hdr_type::ContentLength);

    // now, let's validate we have a packet that contains all the content
    const auto content_begin = found_delims[1].first + found_delims[1].second;
    const auto content_avail = std::ssize(packet) - content_begin;

    // compare the content_end to the size of the packet
    if (content_avail == content_expected_len) {
      // packet contains all the content, create a span representing the content in the packet
      content.assign_span(std::span(packet.from_begin(content_begin), content_expected_len));
    } else {
      // packet does not contain all the content, read bytes from the socket
      const auto more_bytes = content_expected_len - content_avail;

      async_read(asio::transfer_exactly(more_bytes), std::move(e));
      return;
    }
  }

  // ok, we now have the headers and content (if applicable)
  // create and send the reply

  Session::save_packet(packet); // noop when config enable=false

  // update rtsp_ctx
  rtsp_ctx->update_from(headers);

  // create the reply to the request
  Inject inject{.method = headers.method(),
                .path = headers.path(),
                .content = std::move(content),
                .headers = headers,
                .aes_ctx = aes_ctx,
                .rtsp_ctx = rtsp_ctx};

  // create the reply and hold onto the shared_ptr
  auto reply = Factory::create(std::move(inject));

  // build the reply from the reply shared_ptr
  auto &reply_packet = reply->build();

  if (reply_packet.size() == 0) { // warn of empty packet
    INFO(module_id, "SEND REPLY", "empty reply method={} path={}\n", reply->method(),
         reply->path());

    async_read(transfer_initial());
    return;
  }

  save_packet(reply_packet);

  // reply has content to send
  aes_ctx.encrypt(reply_packet); // NOTE: noop until cipher exchange completed

  // must capture reply to ensure it stays in scope until the async completes
  auto reply_buffer = asio::buffer(reply_packet);

  asio::async_write(
      sock,         // sock to send reply
      reply_buffer, //
      [s = ptr(), e = std::move(e), reply = std::move(reply)](error_code ec, ssize_t bytes) {
        Elapsed e2(e); // make a local copy to avoid mutable lambda

        const auto msg = io::is_ready(s->sock, ec);

        if (!msg.empty()) {
          INFO(module_id, "WRITE_REPLY", "{}\n", msg);
        } else {

          Stats::write(stats::RTSP_SESSION_MSG_ELAPSED, e2.freeze());
          Stats::write(stats::RTSP_SESSION_TX_REPLY, bytes);

          // message handling complete, reset buffers
          s->wire = Wire();
          s->packet = Packet();
          s->headers = Headers();
          s->content = Content();

          // continue processing messages
          s->async_read(transfer_initial());
        }
      });
}

void Session::save_packet(Session::Packet &packet) noexcept { // static
  if (Config().at("debug.rtsp.save"sv).value_or(false)) {
    const auto base = Config().at("debug.path"sv).value_or(string());
    const auto file = Config().at("debug.rtsp.file"sv).value_or(string());

    namespace fs = std::filesystem;
    fs::path path{fs::current_path()};
    path.append(base);

    try {
      fs::create_directories(path);

      fs::path full_path(path);
      full_path.append(file);

      std::ofstream out(full_path, std::ios::app);

      out.write(packet.raw<char>(), packet.size());

    } catch (const std::exception &e) {
      INFO(module_id, "RTSP_SAVE", "exception, {}\n", e.what());
    }
  }
}

} // namespace rtsp
} // namespace pierre
