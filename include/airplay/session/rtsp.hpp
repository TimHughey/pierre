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
#include "base/content.hpp"
#include "base/headers.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "common/ss_inject.hpp"
#include "core/host.hpp"
#include "reply/inject.hpp"
#include "session/base.hpp"

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

class Rtsp : public Base, public std::enable_shared_from_this<Rtsp> {
public:
  enum DumpKind { RawOnly, HeadersOnly, ContentOnly };

public:
  ~Rtsp() { teardown(); }
  static shRtsp start(const Inject &di) {
    // creates the shared_ptr and starts the async loop
    // the asyncLoop holds onto the shared_ptr until an error on the
    // socket is detected
    auto session = shRtsp(new Rtsp(di));

    session->asyncLoop();

    return session;
  }

private:
  Rtsp(const Inject &di)
      : Base(di, csv("RTSP SESSION")),   // Base holds the newly connected socket
        aes_ctx(Host::ptr()->deviceID()) // create aes ctx
  {
    __LOG0(LCOL01 " handle={}\n", moduleID(), "NEW", socket.native_handle());
  }

public:
  // initiates async request run loop
  void asyncLoop() override; // see .cpp file for critical function details

  // Getters
  const Content &content() const { return _content; }
  const Headers &headers() const { return _headers; }
  csv method() const { return _headers.method(); }
  csv path() const { return _headers.path(); }
  csv protocol() const { return _headers.protocol(); }

private:
  bool createAndSendReply();
  bool ensureAllContent(); // uses Headers to ensure all content is loaded

  // receives the rx_bytes from async_read
  void handleRequest(size_t bytes);
  bool rxAvailable(); // load bytes immediately available
  uint8v &wire() { return _wire; }

  // misc debug / logging
  void dump(DumpKind dump_type = RawOnly);
  void dump(const auto *data, size_t len) const;
  void infoNewSession() const;

private:
  // order dependent - initialized by constructor
  AesCtx aes_ctx;

  uint8v _wire;   // plain text or ciphered
  uint8v _packet; // deciphered
  Headers _headers;
  Content _content;

  string active_remote;
};

} // namespace session
} // namespace airplay
} // namespace pierre