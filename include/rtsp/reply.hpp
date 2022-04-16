/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include <array>
#include <cctype>
#include <fmt/core.h>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "core/host.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/content.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/nptp.hpp"
#include "rtsp/reply/method.hpp"
#include "rtsp/reply/packet_out.hpp"
#include "rtsp/reply/path.hpp"

namespace pierre {
namespace rtsp {
using Service = pierre::Service;
using std::string, std::string_view, std::tuple, std::unordered_map,
    std::vector;

// Building the response
// 1. Include CSeq header from request
// 2. Include Server header
//

// Actual Payload Format
// RTSP/1.0 200 OK\r\n
// CSeq: <from request>
// Content-Type: <based on content>
// Header1: Value1\r\n
// Header2: Value2\r\n
// <more headers each followed by \r\n>
// Content-Length: <val>  <<-- if there is content
// \r\n  <<-- separate headers from content
// <binary or plist content>
//
// >> write data to socket <<

class Reply; // forward decl for shared_ptr typedef
typedef std::shared_ptr<Reply> sReply;

typedef std::tuple<char *, size_t> PacketInInfo;
typedef const unordered_map<RespCode, const char *> RespCodeMap;

class Reply : public Headers, protected reply::Method, protected reply::Path {
public:
  using string = std::string;

  struct Opts {
    using string = std::string;

    const string &method;
    const string &path;
    const Content &content;
    const Headers &headers;
    sHost host = nullptr;
    sService service = nullptr;
    sAesCtx aes_ctx = nullptr;
    smDNS mdns = nullptr;
    sNptp nptp = nullptr;
  };

public:
  // static member function calls Factory to create the appropriate Reply
  // subclass
  [[nodiscard]] static sReply create(const Opts &opts);

public:
  // construct a new Reply
  // minimum args: method, path
  Reply(const Opts &opts);

  sAesCtx aesCtx() { return _aes_ctx; }
  PacketOut &build();
  void copyToContent(const uint8_t *begin, const size_t bytes);
  void dump() const;
  auto &errMsg() { return _err_msg; }
  sHost host() { return _host; }
  smDNS mdns() { return _mdns; }
  sNptp nptp() { return _nptp; }

  virtual bool populate() = 0;
  inline void responseCode(RespCode code) { _rcode = code; }
  inline const Content &requestContent() const { return _request_content; }
  sService service() { return _service; }

protected:
  string _err_msg;

  RespCode _rcode = RespCode::NotImplemented;

  sHost _host;
  sService _service;
  sAesCtx _aes_ctx;
  smDNS _mdns;
  sNptp _nptp;
  const Content &_request_content;

  Content _content;
  PacketOut _packet;
};

} // namespace rtsp
} // namespace pierre