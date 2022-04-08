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
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "rtsp/headers.hpp"

namespace pierre {
namespace rtsp {

using namespace std;

class Request; // forward decl for shared_ptr typedef

typedef std::shared_ptr<Request> RequestShared;

class Request : std::enable_shared_from_this<Request>, public Headers {
public:
  typedef std::array<char, 512> Raw;
  typedef std::vector<uint8_t> Content;

public:
  enum DumpKind { RawOnly, HeadersOnly, ContentOnly };

public:
  [[nodiscard]] static RequestShared create() {
    // Not using std::make_shared<Best> because the c'tor is private.
    return RequestShared(new Request());
  }

  Raw &buffer() { return _storage; }
  Content &content() { return _content; }
  void contentError(const string ec_msg) { _msg_content = ec_msg; }

  void dump(DumpKind dump_type);

  RequestShared getPtr() { return shared_from_this(); }

  const string &method() const { return _method; }

  void parse();
  const string &path() const { return _path; }

  void sessionStart(const size_t bytes, string ec_msg) {
    _bytes = bytes;
    _session_msg = ec_msg;
  }

  auto shouldLoadContent() { return (_content_length != 0) && content().empty(); }

private:
  Request();

  auto findPlist(const auto bol, const auto abs_end) const;
  auto importPlist(const auto plist_start, const auto plist_end);

  void parseHeader(const string &line);
  void parseMethod(const string &line);

  inline bool printable(const char ch) {
    auto uc = static_cast<unsigned char>(ch);

    return std::isprint(uc);
  }

private:
  Raw _storage{0};
  size_t _bytes = 0;
  string _method;
  string _path;
  string _protocol;
  size_t _content_length = 0;

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