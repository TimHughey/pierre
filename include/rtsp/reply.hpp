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
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "core/host.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "packet/content.hpp"
#include "packet/headers.hpp"
#include "packet/out.hpp"
#include "packet/resp_code.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/reply/method.hpp"
#include "rtsp/reply/path.hpp"

namespace pierre {
namespace rtsp {
// using std::string, std::tuple, std::unordered_map, std::vector;

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

class Reply : protected reply::Method, protected reply::Path {
public:
  using string = std::string;
  using string_view = std::string_view;
  using enum packet::RespCode;
  struct Opts {
    const string_view method;
    const string_view path;
    const packet::Content &content;
    const packet::Headers &headers;
    sHost host = nullptr;
    sService service = nullptr;
    sAesCtx aes_ctx = nullptr;
    smDNS mdns = nullptr;
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
  packet::Out &build();

  void copyToContent(const fmt::memory_buffer &buf);
  void copyToContent(std::shared_ptr<uint8_t[]> data, const size_t bytes);
  void copyToContent(const uint8_t *begin, const size_t bytes);

  void dump() const;
  auto &errMsg() { return _err_msg; }

  sHost host() { return _host; }

  bool debugFlag(bool debug_flag) {
    _debug_flag = debug_flag;
    return _debug_flag;
  }

  bool debug() const { return _debug_flag; }

  smDNS mdns() { return _mdns; }

  virtual bool populate() = 0;

  inline const packet::Content &plist() const { return _request_content; }

  inline void responseCode(packet::RespCode code) { _rcode = code; }
  inline string_view responseCodeView() const { return respCodeToView(_rcode); }

  inline const packet::Content &rContent() const { return _request_content; }
  inline const packet::Headers &rHeaders() const { return _request_headers; };

  sService service() { return _service; }

  // sequence number of this request/reply exchange
  size_t sequence() { return headers.getValInt(packet::Headers::Type2::CSeq); };

protected:
  // misc
  const std::source_location
  __here(std::source_location loc = std::source_location::current()) const {
    return loc;
  };

  const char *fnName(std::source_location loc = std::source_location::current()) const {
    return __here(loc).function_name();
  }

protected:
  string _err_msg;

  packet::RespCode _rcode = packet::RespCode::NotImplemented;

  sHost _host;
  sService _service;
  sAesCtx _aes_ctx;
  smDNS _mdns;

  const packet::Content &_request_content;
  const packet::Headers &_request_headers;

  packet::Headers headers;

  packet::Content _content;
  packet::Out _packet;

  bool _debug_flag = true;
};

} // namespace rtsp
} // namespace pierre