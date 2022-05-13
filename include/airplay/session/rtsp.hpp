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

#pragma once

#include "aes/aes_ctx.hpp"
#include "common/anchor.hpp"
#include "common/conn_info.hpp"
#include "common/ss_inject.hpp"
#include "packet/content.hpp"
#include "packet/headers.hpp"
#include "packet/in.hpp"
#include "reply/inject.hpp"

#include <fmt/format.h>
#include <memory>

namespace pierre {
namespace airplay {
namespace session {

namespace {
namespace errc = boost::system::errc;
}

// forward decl for shared_ptr def
class Rtsp;

typedef std::shared_ptr<Rtsp> shRtsp;

class Rtsp : public std::enable_shared_from_this<Rtsp> {
public:
  enum DumpKind { RawOnly, HeadersOnly, ContentOnly };

public:
  ~Rtsp() { teardown(); }
  static void start(const Inject &di) {
    // creates the shared_ptr and starts the async loop
    // the asyncLoop holds onto the shared_ptr until an error on the
    // socket is detected
    shRtsp(new Rtsp(di))->asyncLoop();
  }

private:
  Rtsp(const Inject &di)
      : socket(std::move(di.socket)),             // newly opened socket for session
        aes_ctx(AesCtx(di.conn.host.deviceID())), // create aes ctx
        conn(di.conn),    // connection info (includes host, service and mDNS
        anchor(di.anchor) // anchor info (not included in ConnInfo)
  {
    infoNewSession();
  }

public:
  // initiates async request run loop
  void asyncLoop(); // see .cpp file for critical function details
  void teardown(const error_code ec);
  void teardown();

  // Getters
  const packet::Content &content() const { return _content; }
  const packet::Headers &headers() const { return _headers; }
  csv method() const { return _headers.method(); }
  csv path() const { return _headers.path(); }
  csv protocol() const { return _headers.protocol(); }

private:
  void accumulateRx(size_t bytes) { _rx_bytes += bytes; }
  void accumulateTx(size_t bytes) { _tx_bytes += bytes; }
  bool createAndSendReply();
  bool ensureAllContent(); // uses Headers functionality to ensure all content loaded
  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec);

  // receives the rx_bytes from async_read
  void handleRequest(size_t bytes);
  bool rxAvailable(); // load bytes immediately available
  bool txContinue();  // send Continue reply
  packet::In &wire() { return _wire; }

  // misc debug / logging
  void dump(DumpKind dump_type = RawOnly);
  void dump(const auto *data, size_t len) const;
  void infoNewSession() const;

private:
  // order dependent - initialized by constructor
  tcp_socket socket;
  AesCtx aes_ctx;
  ConnInfo &conn;
  Anchor &anchor;

  packet::In _wire;   // plain text or ciphered
  packet::In _packet; // deciphered
  packet::Headers _headers;
  packet::Content _content;

  uint64_t _rx_bytes = 0;
  uint64_t _tx_bytes = 0;

  bool _shutdown = false;
};

} // namespace session
} // namespace airplay
} // namespace pierre