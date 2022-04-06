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

#include <algorithm>
#include <ctime>
#include <fmt/format.h>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <typeinfo>

#include <rtsp/request.hpp>

namespace pierre {
namespace rtsp {

using namespace std;

Request::Request() {}

void Request::dump(DumpKind dump_type) {
  std::time_t now = std::time(nullptr);
  std::string dt = std::ctime(&now);

  fmt::print("\n>>> {}", dt);

  if (dump_type == HeadersOnly) {
    fmt::print("\n{} bytes={:04} ", (_ok ? "OK" : "FAILURE"), _bytes);
    fmt::print("method={} path={} protocol={} ", _method, _path, _protocol);
    fmt::print("content_len={:03}\n", _content.size());

    headersDump();
  }

  if (dump_type == ContentOnly) {

    auto &content_type = headerGetValue(ContentType);

    if (content_type.compare("application/octet-stream") == 0) {
      fmt::print("\nContent bytes={:03}\n", _content.size());

      for (size_t byte_num = 0; const auto &byte : _content) {
        fmt::print("{:03}[0x{:02x}] ", byte_num, byte);

        byte_num++;

        if ((byte_num % 10) == 0) {
          fmt::print("\n");
        }
      }
    }
  }

  if (dump_type == RawOnly) {
    fmt::print("\nRAW\n---\n");
    auto out = string_view{_storage.data(), _bytes};
    fmt::print("{:s}\n---\n", out);
  }

  fmt::print("\n");
}

size_t Request::parsePreamble() {
  auto abs_start = _storage.begin();
  auto abs_end = _storage.begin();

  advance(abs_end, _bytes); // only consider the actual bytes recv'ed
  auto fuzzy_end = abs_end; // adjusted when content length is discovered

  for (auto bol = abs_start; printable(*bol) && (bol < fuzzy_end);) {

    // find the end of line
    constexpr auto CR = 0x0a;
    auto eol = find_if(bol, fuzzy_end, [](const auto i) { return i == CR; });

    if (eol != fuzzy_end) {
      const auto line = string{bol, eol};

      if (_method.empty()) {
        parseMethod(line);
      } else {
        parseHeader(line);
      }
    }

    bol = std::next(eol); // skip the end of line character
  }

  return _content_length;
}

void Request::parseHeader(const string &line) {

  // Example Headers
  //
  // Content-Type: application/octet-stream
  // Content-Length: 16
  // User-Agent: Music/1.2.2 (Macintosh; OS X 12.2.1) AppleWebKit/612.4.9.1.8
  // Client-Instance: BAFE421337BA1913

  // NOTE: index 0 is entire string
  constexpr auto key_idx = 1;
  constexpr auto val_idx = 2;
  constexpr auto match_count = 3;

  // headers of note
  constexpr auto key_content_len = "Content-Length";

  smatch sm;
  auto re_method = regex{"([\\w-]*): (\\w[^\\r\\n]*)", re_syntax};
  regex_search(line, sm, re_method);

  _ok = sm.ready();

  if (_ok && (sm.size() == match_count)) {
    const auto &key = sm[key_idx].str();
    const auto &val = sm[val_idx].str();

    headerAdd(key, val);

    if (key.compare(key_content_len) == 0) {
      const auto len = atoi(val.c_str());
      if (len > 0) {
        _content.resize(len);
      }

      // save the numeric value of content length
      _content_length = len;
    }
  }
}

void Request::parseMethod(const string &line) {
  using namespace std::literals;

  // Example Method
  //
  // POST /fp-setup RTSP/1.0

  // NOTE: index 0 is entire string
  constexpr auto method_idx = 1;
  constexpr auto path_idx = 2;
  constexpr auto protocol_idx = 3;
  constexpr auto match_count = 4;

  fmt::print("\t\t\t{}\n", line);

  smatch sm;
  // static auto re_method = regex{"([A-Z]*) (.[a-zA-Z-_]*) (.*)", re_syntax};
  static auto re_method = regex(R"(([A-Z]*) (.*) (RTSP.*))", re_syntax);
  regex_search(line, sm, re_method);

  _ok = sm.ready();

  if (_ok && (sm.size() == match_count)) {
    _method = sm[method_idx].str();
    _path = sm[path_idx].str();
    _protocol = sm[protocol_idx].str();
  }
}

} // namespace rtsp
} // namespace pierre