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
#include <fmt/format.h>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "core/service.hpp"
#include "rtsp/aes_ctx.hpp"
#include "rtsp/content.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/packet_in.hpp"

namespace pierre {
namespace rtsp {

using namespace std;
using namespace core;

class Request; // forward decl for shared_ptr typedef

typedef shared_ptr<Request> sRequest;

class Request : public std::enable_shared_from_this<Request>, public Headers {
public:
  enum DumpKind { RawOnly, HeadersOnly, ContentOnly };

public:
  [[nodiscard]] static sRequest create(sAesCtx ctx, sService service) {
    // Not using std::make_shared<Best> because the c'tor is private.

    return sRequest(new Request(ctx, service));
  }

  sRequest getPtr() { return shared_from_this(); }

  // general API

  sAesCtx aesContext() { return _aes_ctx; }
  Content &content() { return _content; }
  void contentError(const string ec_msg) { _msg_content = ec_msg; }
  void dump(DumpKind dump_type = RawOnly);
  void dump(const auto *data, size_t len) const;
  const string &method() const { return _method; }
  PacketIn &packet() { return _packet; }
  void parse();
  const string &path() const { return _path; }
  sService service() { return _service; }
  void sessionStart(const size_t bytes, string ec_msg);
  bool shouldLoadContent();

private:
  Request(sAesCtx aes_ctx, sService service);

  auto findPlist(const auto bol, const auto abs_end) const;
  bool findSeparator();
  // auto importPlist(const auto plist_start, const auto plist_end);
  void parseHeader(const string &line);
  void parseMethod(const string &line);

  // inline bool printable(const char ch) {
  //   return std::isprint(static_cast<unsigned char>(ch));
  // }

private:
  sAesCtx _aes_ctx;
  sService _service;
  PacketIn _packet;
  size_t _bytes = 0;
  string _method;
  string _path;
  string _protocol;
  size_t _content_length = 0;

  size_t _header_bytes = 0;
  size_t _content_offset = 0;

  Content _content;

  string _session_msg;
  string _msg_parse;
  string _msg_content;
  bool _ok = false;

private:
  static constexpr auto re_syntax = regex_constants::ECMAScript;
};

} // namespace rtsp

} // namespace pierre