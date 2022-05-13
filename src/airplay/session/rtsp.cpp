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
#include "packet/content.hpp"
#include "reply/reply.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <regex>

namespace pierre {
namespace airplay {
namespace session {

using namespace reply;
using namespace boost::asio;
using namespace boost::system;

constexpr auto re_syntax = std::regex_constants::ECMAScript;

// notes:
//  1. socket is passed as a reference to a reference so we must move to our local socket reference
// Rtsp::Rtsp(tcp_socket &&new_socket, const Rtsp::Opts &opts)
//     : socket(std::move(new_socket)),            // io_context / socket for this session
//       host(opts.host),                          // the Host (config, platform info)
//       service(opts.service),                    // service definition for mDNS (used by Reply)
//       mdns(opts.mdns),                          // mDNS (used by Reply)
//       aes_ctx(AesCtx::create(host->deviceID())) // cipher
// {
//   fmt::print("{} socket={}\n", fnName(), socket.native_handle());
// }

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
  //     waiting for data on the socket then during execution
  //     of the lamba
  //
  //  4. when called again from within the lamba the sequence of
  //     events repeats (this function returns) and the shared_ptr
  //     once again goes out of scope
  //
  //  5. the crucial point -- we must keep the use count
  //     of the session above zero until the session ends
  //     (e.g. due to error, natural completion, io_ctx is stopped)

  async_read(socket,                 // rx from this socket
             dynamic_buffer(wire()), // put rx'ed data here
             transfer_at_least(24),  // start by rx'ed this many bytes
             [self = shared_from_this()](error_code ec, size_t rx_bytes) {
               // general notes:
               //
               // 1. this was not captured so the lamba is not in 'this' context
               // 2. all calls to the session must be via self (which was captured)
               // 3. we do minimal activities to quickly get to 'this' context

               // essential activies:
               //
               // 1. check the error code
               if (self->isReady(ec)) {
                 // 2. handle the request
                 //    a. remember... the data received is in wire buffer
                 //    b. performs request if bytes rx'ed != content length header
                 //    c. schedules more io_ctx work or allows shared_ptr to fall out of
                 //    scope
                 self->handleRequest(rx_bytes);
               } // self is about to go out of scope...
             }); // self is out of scope and the shared_ptr use count is reduced by one

  // final notes:
  // 1. the first return of this function traverses back to the Server that created the
  //    Rtsp (in the same io_ctx)
  // 2. subsequent returns are to the io_ctx and match the required void return signature
}

void Rtsp::handleRequest(size_t rx_bytes) {
  accumulateRx(rx_bytes);

  try {
    if (rxAvailable()            // drained the socket successfully
        && ensureAllContent()    // loaded bytes == Content-Length
        && createAndSendReply()) // sent the reply OK
    {                            // all is well, prepare for next request
      _wire.clear();
      _packet.clear();
      _headers.clear();
      _content.clear();

      asyncLoop(); // schedule rx of next packet
    }
  } catch (const std::exception &e) {
    constexpr auto f = FMT_STRING("{} RTSP REPLY FAILURE "       // failure
                                  "method={} path={} reason={} " // debug info
                                  "(header and content dump follow)\n\n");

    fmt::print(f, runTicks(), _headers.method(), _headers.path(), e.what());

    _headers.dump();
    _content.dump();

    fmt::print("\n{} RTSP REPLY FAILURE END\n\n", runTicks());
  }
}

bool Rtsp::rxAvailable() {
  error_code ec;
  size_t avail_bytes = socket.available(ec);

  auto buff = dynamic_buffer(_wire);

  while (isReady(ec) && (avail_bytes > 0)) {
    auto tx_bytes = read(socket, buff, transfer_at_least(avail_bytes), ec);
    accumulateTx(tx_bytes);

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

  if (false) { // debug
    if (more_bytes) {
      constexpr auto f = FMT_STRING("{} RTSP SESSION NEED MORE bytes={} method={} path={}\n");
      fmt::print(f, runTicks(), more_bytes, _headers.method(), _headers.path());
    }
  }

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

  if (false) { // debug
    if (rc) {
      constexpr auto f = FMT_STRING("{} RTSP SESSION RX total_bytes={}\n");
      fmt::print(f, runTicks(), _wire.size());
    }
  }

  return rc;
}

bool Rtsp::createAndSendReply() {
  if (isReady() == false) {
    return false;
  }

  // create the reply to the request
  auto inject = reply::Inject{.method = method(),
                              .path = path(),
                              .content = content(),
                              .headers = headers(),
                              .conn = conn,
                              .aes_ctx = aes_ctx,
                              .anchor = anchor};

  auto reply = Reply::create(inject);

  auto &reply_packet = reply->build();

  if (reply_packet.size() == 0) { // warn of empty packet, still return true
    constexpr auto f = FMT_STRING("{} RTSP SESSION EMPTY REPLY method={} path={}\n");
    fmt::print(f, runTicks(), inject.method, inject.path);

    return true;
  }

  // reply has content to send
  aes_ctx.encrypt(reply_packet); // NOTE: noop until cipher exchange completed
  auto buff = const_buffer(reply_packet.data(), reply_packet.size());

  error_code ec;
  auto tx_bytes = write(socket, buff, ec);
  accumulateTx(tx_bytes);

  // check the most recent ec
  return isReady(ec);
}

bool Rtsp::isReady(const error_code &ec) {
  auto rc = true;

  switch (ec.value()) {
    case errc::success:
      break;

    case errc::operation_canceled:
    case errc::resource_unavailable_try_again:
    case errc::no_such_file_or_directory:
    default:
      teardown(ec); // problem... teardown
      rc = false;
  }

  return rc;
}

void Rtsp::teardown() {
  if (socket.is_open()) {
    [[maybe_unused]] error_code ec;

    socket.shutdown(tcp_socket::shutdown_both, ec);
    socket.close(ec);

    constexpr auto f = FMT_STRING("{} RTSP SESSION SHUTDOWN socket={} err_value={} msg={}\n");
    fmt::print(f, runTicks(), socket.native_handle(), ec.value(), ec.message());
  } else {
    constexpr auto f = FMT_STRING("{} RTSP SESSION PREVIOUS TEARDOWN socket={}\n");
    fmt::print(f, runTicks(), socket.native_handle());
  }
}

void Rtsp::teardown(const error_code ec) {
  constexpr auto f = FMT_STRING("{} RTSP SESSION TEARDOWN socket={} err_value={} msg={}\n");
  fmt::print(f, runTicks(), socket.native_handle(), ec.value(), ec.message());

  teardown();
}

bool Rtsp::txContinue() {
  // send a CONTINUE reply then read the pending data
  constexpr auto CONTINUE = csv("CONTINUE");

  auto inject = reply::Inject{.method = CONTINUE,
                              .path = path(),
                              .content = content(),
                              .headers = headers(),
                              .conn = conn,
                              .aes_ctx = aes_ctx,
                              .anchor = anchor};

  auto reply = Reply::create(inject);
  auto &reply_packet = reply->build();

  aes_ctx.encrypt(reply_packet); // NOTE: noop until cipher exchange completed
  auto buff = const_buffer(reply_packet.data(), reply_packet.size());

  error_code ec;
  auto tx_bytes = write(socket, buff, ec);
  accumulateTx(tx_bytes);

  return isReady(ec);
}

// misc debug, loggind

void Rtsp::dump(DumpKind dump_type) {
  std::time_t now = std::time(nullptr);
  std::string dt = std::ctime(&now);

  if (dump_type == HeadersOnly) {
    fmt::print("method={} path={} protocol={} {}\n", method(), path(), protocol(), dt);

    _headers.dump();
  }

  if (dump_type == ContentOnly) {
    _content.dump();
  }

  if (dump_type == RawOnly) {
    csv out((const char *)_wire.data(), _wire.size());
    fmt::print("{}\n", out);
  }

  fmt::print("\n");
}

void Rtsp::infoNewSession() const {
  constexpr auto f = "{} {} new session\n";
  fmt::print(f, runTicks(), fnName());
}

} // namespace session
} // namespace airplay
} // namespace pierre