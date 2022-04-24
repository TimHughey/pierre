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

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <regex>
#include <source_location>
#include <string_view>
#include <thread>

#include "rtp/rtp.hpp"
#include "rtsp/content.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/session.hpp"

namespace pierre {
namespace rtsp {

using namespace boost::asio;
using namespace boost::system;
using namespace pierre::rtp;
using error_code = boost::system::error_code;
using src_loc = std::source_location;
using string_view = std::string_view;

using enum Headers::Type2;

Session::Session(tcp_socket &socket, const Session::Opts &opts)
    : socket(socket),        // io_context / socket for this session
      host(opts.host),       // the Host (config, platform info)
      service(opts.service), // service definition for mDNS (used by Reply)
      mdns(opts.mdns),       // mDNS (used by Reply)
      nptp(opts.nptp),       // Nptp (used by Reply)
      rtp(Rtp::create()),    // Real Time Protocol (used by Reply to create audio)
      aes_ctx(AesCtx::create(host->deviceID())) // cipher
{
  // maybe more
}

size_t Session::decrypt(PacketIn &packet) {
  auto consumed = aes_ctx->decrypt(packet, _wire);
  _wire.clear(); // wire bytes have been deciphered, clear them

  return consumed;
}

void Session::readApRequest() {
  // bail out if the socket isn't ready
  if (isReady() == false) {
    return;
  }

  // this function is recursive via the lambda, reset stateful objects
  _wire.clear();
  _headers.clear();
  _content.clear();

  auto buff = dynamic_buffer(_wire);

  async_read(socket, buff, boost::asio::transfer_at_least(16),
             [&](error_code ec, size_t rx_bytes) {
               accumulateRx(rx_bytes);

               if (isReady(ec) == false) {
                 return;
               }

               rxAvailable();      // drain the socket
               ensureAllContent(); // load additional content

               try {
                 createAndSendReply();

                 readApRequest();
               } catch (const std::exception &e) {
                 fmt::print("{} create reply failed: {}\n", fnName(), e.what());

                 _headers.dump();
                 _content.dump();
               }
             });
}

bool Session::rxAvailable() {
  if (isReady()) {
    error_code ec;
    size_t avail_bytes = socket.available(ec);

    auto buff = dynamic_buffer(_wire);

    while (isReady(ec) && (avail_bytes > 0)) {
      auto tx_bytes = read(socket, buff, transfer_at_least(avail_bytes), ec);
      accumulateTx(tx_bytes);

      avail_bytes = socket.available(ec);
    }
  }

  return isReady();
}

void Session::ensureAllContent() {
  // bail out if the socket isn't ready
  if (isReady() == false) {
    return;
  }

  // if subsequent data is loaded it will all need up in this packet
  auto packet = PacketIn();

  // decrypt the wire bytes resceived thus far
  [[maybe_unused]] auto consumed = decrypt(packet);

  // need more content? send Continue reply
  auto more_bytes = _headers.loadMore(packet.view(), _content);

  // if more bytes are needed, reply Continue
  if (more_bytes) {
    auto reply = Reply::create({.method = method(),
                                .path = path(),
                                .content = content(),
                                .headers = headers(),
                                .host = host,
                                .service = service,
                                .aes_ctx = aes_ctx,
                                .mdns = mdns,
                                .nptp = nptp,
                                .rtp = rtp});

    auto &reply_packet = reply->build();

    aes_ctx->encrypt(reply_packet); // NOTE: noop until cipher exchange completed
    auto buff = const_buffer(reply_packet.data(), reply_packet.size());

    error_code ec;
    auto tx_bytes = write(socket, buff, ec);
    accumulateTx(tx_bytes);
  }

  // if wire bytes are loaded remember this is an entirely new packet
  while (more_bytes && isReady()) {
    if (rxAtLeast(more_bytes)) {
      [[maybe_unused]] auto consumed = decrypt(packet);
      more_bytes = _headers.loadMore(packet.view(), _content);
    }
  }
}

void Session::createAndSendReply() {
  if (isReady() == false) {
    return;
  }

  // create the reply to the request
  auto reply = Reply::create({.method = method(),
                              .path = path(),
                              .content = content(),
                              .headers = headers(),
                              .host = host,
                              .service = service,
                              .aes_ctx = aes_ctx,
                              .mdns = mdns,
                              .nptp = nptp,
                              .rtp = rtp});

  auto &reply_packet = reply->build();

  if (reply->log()) {
    fmt::print("{} reply seq={} method={} path={} resp_code={}\n", fnName(), reply->sequence(),
               method(), path(), reply->responseCodeView());
  }

  // only send the reply packet if there's data
  if (reply_packet.size() > 0) {
    aes_ctx->encrypt(reply_packet); // NOTE: noop until cipher exchange completed
    auto buff = const_buffer(reply_packet.data(), reply_packet.size());

    error_code ec;
    auto tx_bytes = write(socket, buff, ec);
    accumulateTx(tx_bytes);

    // check the most recent ec
    isReady(ec);
  }
}

bool Session::isReady(const error_code &ec, const src_loc loc) {
  auto rc = isReady();

  if (rc) {
    switch (ec.value()) {
      case errc::success:
        break;

      default:
        fmt::print("{} closing socket={} err_value={} msg={}\n", loc.function_name(),
                   socket.native_handle(), ec.value(), ec.message());

        socket.shutdown(tcp_socket::shutdown_both);
        socket.close();
        rc = false;
    }
  }

  return rc;
}

bool Session::rxAtLeast(size_t bytes) {
  if (isReady()) {
    auto buff = dynamic_buffer(_wire);

    error_code ec;
    auto rx_bytes = read(socket, buff, transfer_at_least(bytes), ec);
    accumulateRx(rx_bytes);
  }
  return isReady();
}

void Session::start() {
  readApRequest();

  const auto id = pthread_self();

  fmt::print("{} thread ID={} returning from start\n", fnName(), id);
}

void Session::dump(DumpKind dump_type) {
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
    const std::string_view out((const char *)_wire.data(), _wire.size());
    fmt::print("{}\n", out);
  }

  fmt::print("\n");
}

} // namespace rtsp
} // namespace pierre