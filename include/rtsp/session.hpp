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

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <regex>
#include <source_location>
#include <string_view>
#include <vector>

#include "core/host.hpp"
#include "mdns/mdns.hpp"
#include "rtp/rtp.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/content.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/nptp.hpp"
#include "rtsp/packet_in.hpp"

namespace pierre {
namespace rtsp {

// forward decl for shared_ptr def
class Session;

typedef std::shared_ptr<Session> sSession;

class Session : public std::enable_shared_from_this<Session> {
public:
  enum DumpKind { RawOnly, HeadersOnly, ContentOnly };

public:
  using error_code = boost::system::error_code;
  using tcp_socket = boost::asio::ip::tcp::socket;
  using string = std::string;
  using string_view = std::string_view;
  using src_loc = std::source_location;

public:
  struct Opts {
    sHost host = nullptr;
    sService service = nullptr;
    smDNS mdns = nullptr;
    sNptp nptp = nullptr;
  };

public:
  // shared_ptr API
  [[nodiscard]] static sSession create(tcp_socket &socket, const Opts &opts) {
    // must call constructor directly since it's private
    return sSession(new Session(socket, opts));
  }

  sSession getPtr() { return shared_from_this(); }

private:
  Session(tcp_socket &socket, const Opts &opts);

public:
  // primary entry point
  void start();

  void dump(DumpKind dump_type = RawOnly);
  void dump(const auto *data, size_t len) const;

  // const PacketIn &packet() const { return _packet; }
  const string_view packetView() const { return _packet.view(); }

  // Getters
  const Content &content() const { return _content; }
  const Headers &headers() const { return _headers; }
  const string_view method() const { return _headers.method(); }
  const string_view path() const { return _headers.path(); }
  const string_view protocol() const { return _headers.protocol(); }

private:
  void accumulateRx(size_t bytes) { _rx_bytes += bytes; }
  void accumulateTx(size_t bytes) { _tx_bytes += bytes; }
  void ensureAllContent(); // uses Headers functionality to ensure all content loaded
  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec, const src_loc loc = src_loc::current());
  void readApRequest(); // returns when socket is closed
  void readAvailable(); // load bytes immediately available
  void createAndSendReply();

  // misc
  const std::source_location here(src_loc loc = src_loc::current()) const { return loc; };

  const char *fnName(src_loc loc = src_loc::current()) const { return here(loc).function_name(); }

  // void log(fmt::string_view format, fmt::format_args args,
  //          const std::source_location loc = std::source_location::current());

private:
  // order dependent - initialized by constructor
  tcp_socket &socket;
  sHost host;
  sService service;
  smDNS mdns;
  sNptp nptp;
  sRtp rtp;

  rtsp::sAesCtx aes_ctx;

  PacketIn _wire;
  PacketIn _packet;
  Headers _headers;
  Content _content;

  uint64_t _rx_bytes = 0;
  uint64_t _tx_bytes = 0;

  bool _shutdown = false;

private:
  static constexpr auto re_syntax = std::regex_constants::ECMAScript;
};

} // namespace rtsp
} // namespace pierre