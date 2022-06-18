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

#include "aes/aes_ctx.hpp"
#include "packet/content.hpp"
#include "packet/headers.hpp"
#include "packet/out.hpp"
#include "packet/resp_code.hpp"
#include "reply/inject.hpp"

#include <array>
#include <cctype>
#include <fmt/core.h>
#include <memory>
#include <optional>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace pierre {
namespace airplay {
namespace reply {
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
typedef std::shared_ptr<Reply> shReply;

namespace { // unnamed namespace visible only in this file
namespace header = pierre::packet::header;
namespace ranges = std::ranges;
} // namespace

class Reply {
public:
  using enum packet::RespCode;

public:
  // static member function calls Factory to create the appropriate Reply
  // subclass
  [[nodiscard]] static shReply create(const reply::Inject &inject);

public:
  // construct a new Reply and inject dependencies
  Reply() = default;

  packet::Out &build();
  Reply &inject(const reply::Inject &di);
  virtual bool populate() = 0;

  void copyToContent(const auto &buf) { ranges::copy(buf, std::back_inserter(_content)); }

  void copyToContent(std::shared_ptr<uint8_t[]> data, const size_t bytes);
  void copyToContent(const uint8_t *begin, const size_t bytes);

  // access injected dependencies
  auto &aesCtx() { return di->aes_ctx; }
  auto &method() const { return di->method; }
  auto &path() const { return di->path; }
  const packet::Content &plist() const { return di->content; }
  const packet::Content &rContent() const { return di->content; }
  const packet::Headers &rHeaders() const { return di->headers; };

  // direct access to injected dependencies
  const Inject &injected() { return *di; }

  inline void responseCode(packet::RespCode code) { _rcode = code; }
  inline string_view responseCodeView() const { return respCodeToView(_rcode); }

  // sequence number of this request/reply exchange
  size_t sequence() { return headers.getValInt(header::type::CSeq); };

  // misc debug
  bool debugFlag(bool debug_flag);
  bool debug() const { return _debug_flag; }
  void dump() const;
  auto &errMsg() const { return _err_msg; }

protected:
  std::optional<reply::Inject> di; // copy, ok because it's only references

  string _err_msg;

  packet::RespCode _rcode = packet::RespCode::NotImplemented;
  packet::Headers headers;

  packet::Content _content;
  packet::Out _packet;

  bool _debug_flag = false;
};

} // namespace reply
} // namespace airplay
} // namespace pierre