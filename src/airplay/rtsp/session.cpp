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
#include "base/content.hpp"
#include "base/elapsed.hpp"
#include "config/config.hpp"
#include "reply/factory.hpp"
#include "reply/reply.hpp"
#include "stats/stats.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <utility>

namespace pierre {
namespace rtsp {

namespace fs = std::filesystem;

void Session::async_loop() noexcept {
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

  static constexpr ssize_t BYTES_MIN{12};

  asio::async_read(sock,                               // rx from this socket
                   asio::dynamic_buffer(wire()),       // put rx'ed data here
                   asio::transfer_at_least(BYTES_MIN), // rx this many bytes
                   [s = ptr(), e = Elapsed()](error_code ec, ssize_t bytes) {
                     Stats::write(stats::RTSP_SESSION_RX_FIRST, bytes);

                     // general notes:
                     // 1. this was not captured so the lamba is not in 'this' context
                     // 2. all calls to the session must be via self (which was captured)
                     // 3. we do minimal activities to quickly get to 'this' context

                     // essential activities:
                     // 1. check the error code
                     if (!ec && s->sock.is_open() && (bytes >= BYTES_MIN)) {
                       // 2. handle the request
                       //    a. remember... the data received is in wire buffer
                       //    b. performs request if bytes rx'ed != content length header
                       //    c. schedules io_ctx work or shared_ptr  falls out of scope

                       if (s->rxAvailable()            // drained the socket successfully
                           && s->ensureAllContent()    // loaded bytes == Content-Length
                           && s->createAndSendReply()) // sent the reply OK
                       {                               // all is well... next request

                         s->_wire.clear();
                         s->_packet.clear();
                         s->_headers.clear();
                         s->_content.clear();

                         s->async_loop(); // schedule rx of next packet
                       } else {
                         io::log_socket_msg(module_id, ec, s->sock, s->sock.remote_endpoint(), e);
                       }

                     } else {
                       io::log_socket_msg(module_id, ec, s->sock, s->sock.remote_endpoint(), e);
                     }
                   }); // self is out of scope and the shared_ptr use count is reduced by one

  // misc notes:
  // 1. the first return of this function traverses back to the Server that
  //    created the Session (in the same io_ctx)
  // 2. subsequent returns are to the io_ctx and match the required void return
  //    signature
}

bool Session::rxAvailable() {
  error_code ec;
  size_t avail_bytes = sock.available(ec);

  while (is_ready(ec) && (avail_bytes > 0)) {
    ssize_t bytes = asio::read(sock,                        //
                               asio::dynamic_buffer(_wire), //
                               asio::transfer_at_least(avail_bytes), ec);

    Stats::write(stats::RTSP_SESSION_RX_AVAIL, bytes);

    avail_bytes = sock.available(ec);
  }

  // decipher what we have into the packet
  [[maybe_unused]] auto consumed = aes_ctx.decrypt(_packet, _wire);

  // note: don't clear _wire since it may contained unciphered bytes
  return is_ready(ec);
}

bool Session::ensureAllContent() {
  auto rc = true;

  // parse packet and return if more bytes are needed (e.g. Content-Length)
  auto more_bytes = _headers.loadMore(_packet.view(), _content);

  INFOX(module_id, "NEED MORE", "bytes={} method={} path={}\n", more_bytes, _headers.method(),
        _headers.path());

  // loop until we have all the data or an error occurs
  while (more_bytes && rc) {
    // send Continue reply indicating packet good so far then read waiting bytes
    if (!rxAvailable()) { // error, bail out
      rc = false;
      continue;
    }

    // additional bytes deciphered and appended to packet
    // check if packet is complete
    more_bytes = _headers.loadMore(_packet.view(), _content);
  }

  INFOX(module_id, "ENSURE CONTENT", "rx total_bytes={}\n", _wire.size());

  return rc;
}

bool Session::createAndSendReply() {
  using namespace airplay::reply;

  Session::save_packet(_packet); // noop if config enable = false

  // create the reply to the request
  Inject inject{.method = _headers.method(),
                .path = _headers.path(),
                .content = _content,
                .headers = _headers,
                .aes_ctx = aes_ctx};

  // create the reply and hold onto the shared_ptr
  auto reply = Factory::create(inject);

  // build the reply from the reply shared_ptr
  auto &reply_packet = reply->build();

  if (reply_packet.size() == 0) { // warn of empty packet, still return true
    INFO(module_id, "SEND REPLY", "empty reply method={} path={}\n", inject.method, inject.path);

    return true;
  }

  // reply has content to send
  aes_ctx.encrypt(reply_packet); // NOTE: noop until cipher exchange completed

  error_code ec;
  [[maybe_unused]] auto tx_bytes =
      asio::write(sock, asio::const_buffer(reply_packet.data(), reply_packet.size()), ec);

  // check the most recent ec
  return is_ready(ec);
}

// misc debug, loggind

void Session::dump(DumpKind dump_type) {

  if (dump_type == HeadersOnly) {
    INFO(module_id, "DUMP HEADERS", "method={} path={} protocol={}\n", _headers.method(),
         _headers.path(), _headers.protocol());

    _headers.dump();
  }

  if (dump_type == ContentOnly) {
    INFO(module_id, "DUMP CONTENT", "method={} path={} protocol={}\n", _headers.method(),
         _headers.path(), _headers.protocol());
    _content.dump();
  }

  if (dump_type == RawOnly) {
    INFO(module_id, "DUMP RAW", "\n{}", _wire.view());
  }

  INFO(module_id, "DUMP_COMPLETE", "method={} path={} protocol={}\n", _headers.method(),
       _headers.path(), _headers.protocol());
}

void Session::save_packet(uint8v &packet) noexcept { // static
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
      out << std::endl << std::endl;

    } catch (const std::exception &e) {
      INFO(module_id, "RTSP_SAVE", "exception, {}\n", e.what());
    }
  }
}

} // namespace rtsp
} // namespace pierre
