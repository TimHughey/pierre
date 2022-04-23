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

// member functions organized by call sequence

void Session::readApRequest() {
  // this function is recursive via the lambda, clean up previous contents
  _packet.clear();
  _headers.clear();
  _content.clear();

  auto buff = dynamic_buffer(_packet);

  async_read(
      socket, buff, boost::asio::transfer_at_least(16), [&](error_code ec, size_t rx_bytes) {
        const std::source_location loc = std::source_location::current();

        if (ec != errc::success) {
          fmt::print("{} failed={} rx_bytes={}\n", loc.function_name(), ec.message(), rx_bytes);
          socket.shutdown(tcp_socket::shutdown_both);
          return;
        }

        if (socket.available()) {
          const auto want_bytes = socket.available();
          error_code ec;
          auto buff = dynamic_buffer(_packet);
          auto rx_bytes = read(socket, buff, transfer_at_least(want_bytes));

          fmt::print("{} read want_bytes={} rx_bytes={}\n", fnName(), want_bytes, rx_bytes);

          if (ec != errc::success) {
            fmt::print("{} read secondary msg={} bytes={}", fnName(), ec.message(), rx_bytes);
            return;
          }
        }

        aes_ctx->decrypt(_packet, _packet.size()); // NOP until encryption available

        // continue loading data until Headers has found everything required
        auto load_more = false;
        do {
          load_more = _headers.loadMore(packetView(), _content);

          if (load_more) {
            error_code ec;
            PacketIn packet_more;

            auto buff = dynamic_buffer(packet_more);
            auto rx_bytes = read(socket, buff, boost::asio::transfer_at_least(1), ec);

            if (ec != errc::success) {
              fmt::print("{} loading more msg={} bytes={}", fnName(), ec.message(), rx_bytes);
              return;
            }

            aes_ctx->decrypt(packet_more, rx_bytes);
            _packet.insert(_packet.cend(), packet_more.begin(), packet_more.end());
          }
        } while (load_more);

        try {
          createAndSendReply();

          readApRequest();
        } catch (const std::exception &e) {
          fmt::print("{} create reply failed: {}\n", fnName(), e.what());

          const size_t len = _packet.size();
          const char *data = (const char *)_packet.data();
          const std::string_view packet_str(data, len);

          _headers.dump();
          _content.dump();
        }
      });
}

void Session::createAndSendReply() {
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
    fmt::print("{} reply method={} path={}\n", fnName(), method(), path());
  }

  // only send the reply packet if there's data
  if (reply_packet.size() > 0) {
    aes_ctx->encrypt(reply_packet); // NOTE: noop until cipher exchange completed
    auto buff = const_buffer(reply_packet.data(), reply_packet.size());

    error_code tx_ec;
    auto tx_bytes = write(socket, buff, tx_ec);

    if ((tx_ec != errc::success) || (tx_bytes != reply_packet.size())) {
      fmt::print("{} failed={} tx_bytes={}\n", fnName(), tx_ec.message(), tx_bytes);

      socket.shutdown(tcp_socket::shutdown_both);
      return;
    }

    _tx_bytes += tx_bytes;
  }
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
    const std::string_view out((const char *)_packet.data(), _packet.size());
    fmt::print("{}\n", out);
  }

  fmt::print("\n");
}

} // namespace rtsp
} // namespace pierre