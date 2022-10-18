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

#include "session/rtsp.hpp"
#include "base/content.hpp"
#include "reply/factory.hpp"
#include "reply/reply.hpp"

#include <algorithm>

namespace pierre {
namespace airplay {
namespace session {

void Rtsp::asyncLoop() {
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

  asio::async_read(socket,                       // rx from this socket
                   asio::dynamic_buffer(wire()), // put rx'ed data here
                   asio::transfer_at_least(12),  // start by rx'ed this many bytes
                   [self = shared_from_this()](error_code ec, size_t rx_bytes) {
                     // general notes:
                     // 1. this was not captured so the lamba is not in 'this' context
                     // 2. all calls to the session must be via self (which was captured)
                     // 3. we do minimal activities to quickly get to 'this' context

                     // essential activities:
                     // 1. check the error code
                     if (self->isReady(ec)) {
                       // 2. handle the request
                       //    a. remember... the data received is in wire buffer
                       //    b. performs request if bytes rx'ed != content length header
                       //    c. schedules io_ctx work or shared_ptr  falls out of scope
                       self->handleRequest(rx_bytes);
                     } // self is about to go out of scope...
                   }); // self is out of scope and the shared_ptr use count is reduced by one

  // misc notes:
  // 1. the first return of this function traverses back to the Server that
  //    created the Rtsp (in the same io_ctx)
  // 2. subsequent returns are to the io_ctx and match the required void return
  //    signature
}

void Rtsp::handleRequest(size_t rx_bytes) {
  accumulate(RX, rx_bytes);

  if (rxAvailable()            // drained the socket successfully
      && ensureAllContent()    // loaded bytes == Content-Length
      && createAndSendReply()) // sent the reply OK
  {                            // all is well... prepare for next request
    _wire.clear();
    _packet.clear();
    _headers.clear();
    _content.clear();

    asyncLoop(); // schedule rx of next packet
  }
}

bool Rtsp::rxAvailable() {
  error_code ec;
  size_t avail_bytes = socket.available(ec);

  auto buff = asio::dynamic_buffer(_wire);

  while (isReady(ec) && (avail_bytes > 0)) {
    auto rx_bytes = asio::read(socket, buff, asio::transfer_at_least(avail_bytes), ec);
    accumulate(RX, rx_bytes);

    avail_bytes = socket.available(ec);
  }

  // decipher what we have into the packet
  [[maybe_unused]] auto consumed = aes_ctx.decrypt(_packet, _wire);

  // note: don't clear _wire since it may contained unciphered bytes
  return isReady(ec);
}

bool Rtsp::ensureAllContent() {
  auto rc = true;

  // parse packet and return if more bytes are needed (e.g. Content-Length)
  auto more_bytes = _headers.loadMore(_packet.view(), _content);

  INFOX(moduleID(), "NEED MORE", "bytes={} method={} path={}\n", more_bytes, _headers.method(),
        _headers.path());

  // loop until we have all the data or an error occurs
  while (more_bytes && rc) {
    // send Continue reply indicating packet good so far then
    // read waiting bytes
    if (!rxAvailable()) { // error, bail out
      rc = false;
      continue;
    }

    // additional bytes deciphered and appended to packet
    // check if packet is complete
    more_bytes = _headers.loadMore(_packet.view(), _content);
  }

  INFOX(moduleID(), "ENSURE CONTENT", "rx total_bytes={}\n", _wire.size());

  return rc;
}

bool Rtsp::createAndSendReply() {
  // create the reply to the request

  reply::Inject inject{.method = method(),
                       .path = path(),
                       .content = content(),
                       .headers = headers(),
                       .aes_ctx = aes_ctx};

  // create the reply and hold onto the shared_ptr
  auto reply = reply::Factory::create(inject);

  // build the reply from the reply shared_ptr
  auto &reply_packet = reply->build();

  if (reply_packet.size() == 0) { // warn of empty packet, still return true
    INFO(moduleID(), "SEND REPLY", "empty reply method={} path={}\n", inject.method, inject.path);

    return true;
  }

  // reply has content to send
  aes_ctx.encrypt(reply_packet); // NOTE: noop until cipher exchange completed
  auto buff = asio::const_buffer(reply_packet.data(), reply_packet.size());

  error_code ec;
  auto tx_bytes = asio::write(socket, buff, ec);
  accumulate(TX, tx_bytes);

  INFOX(moduleID(), "sent tx_bytes={:<4} reason={} method={} path={}\n", moduleID(),
        csv("SEND REPLY"), tx_bytes, ec.message(), inject.method, inject.path);

  // check the most recent ec
  return isReady(ec);
}

// misc debug, loggind

void Rtsp::dump(DumpKind dump_type) {

  if (dump_type == HeadersOnly) {
    INFO(moduleID(), "DUMP HEADERS", "method={} path={} protocol={}\n", method(), path(),
         protocol());

    _headers.dump();
  }

  if (dump_type == ContentOnly) {
    INFO(moduleID(), "DUMP CONTENT", "method={} path={} protocol={}\n", method(), path(),
         protocol());
    _content.dump();
  }

  if (dump_type == RawOnly) {
    INFO(moduleID(), "DUMP RAW", "\n{}", _wire.view());
  }

  INFO(moduleID(), "DUMP_COMPLETE", "method={} path={} protocol={}\n", method(), path(),
       protocol());
}

} // namespace session
} // namespace airplay
} // namespace pierre