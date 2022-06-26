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

#include "packet/headers.hpp"
#include "base/typical.hpp"
#include "packet/content.hpp"
#include "packet/resp_code.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <fmt/format.h>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>

namespace pierre {
namespace packet {

using namespace std::literals;
using namespace std::literals::string_view_literals;
using std::regex;
using std::string, std::string_view, std::unordered_map, std::back_inserter;

namespace ranges = std::ranges;

static std::array __known_types{header::type::CSeq,
                                header::type::Server,
                                header::type::ContentSimple,
                                header::type::ContentType,
                                header::type::ContentLength,
                                header::type::Public,
                                header::type::DacpActiveRemote,
                                header::type::DacpID,
                                header::type::AppleProtocolVersion,
                                header::type::UserAgent,
                                header::type::AppleHKP,
                                header::type::XAppleClientName,
                                header::type::XApplePD,
                                header::type::XAppleProtocolVersion,
                                header::type::XAppleHKP,
                                header::type::XAppleET,
                                header::type::RtpInfo,
                                header::type::XAppleAbsoulteTime};

static string __EMPTY;

void Headers::add(csv type, csv val) {
  auto known_header = ranges::find(__known_types, type);

  if (known_header == __known_types.end()) {
    const auto &[it, __inserted] = _unknown.emplace(string(type));
    _omap.try_emplace(*it, string(val));

  } else {
    _omap.try_emplace(*known_header, string(val));
  }
}

void Headers::add(csv type, size_t val) {
  auto str = fmt::format("{}", val);

  add(type, str);
}

void Headers::copy(const Headers &from, csv type) { add(type, from.getVal(type)); }

void Headers::clear() {
  _omap.clear();
  _separators.clear();

  _method.clear();
  _path.clear();
  _protocol.clear();

  _more_bytes = 0;
  _ok = false;
}

bool Headers::exists(csv type) const { return _omap.contains(type); }

bool Headers::isContentType(csv want_val) const {
  const auto &search = _omap.find(header::type::ContentType);

  if (search == _omap.end())
    return false;

  return want_val == search->second;
}

const string &Headers::getVal(csv want_type) const {
  const auto &search = _omap.find(want_type);

  if (search == _omap.end()) {
    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {} type={} not found (returning empty string)\n");
      fmt::print(f, runTicks(), fnName(), want_type);
    }

    return __EMPTY;
  }

  return search->second;
}

size_t Headers::getValInt(csv want_type) const {
  const auto &search = _omap.find(want_type);

  if (search == _omap.end()) {
    constexpr auto f = FMT_STRING("{} {} type={} not found (returning 0)\n");
    fmt::print(f, runTicks(), fnName(), want_type);

    return 0;

    //  throw(std::runtime_error("header type not found"));
  }

  return static_cast<size_t>(std::atoi(search->second.data()));
}

bool Headers::haveSeparators(const string_view &view) {
  if (_separators.size() == 2) {
    return true;
  }

  const auto want = std::array{EOL, SEP};
  for (size_t search_pos = 0; const auto &it : want) {
    auto pos = view.find(it, search_pos);

    if (pos != view.npos) {
      _separators.emplace_back(pos);
      search_pos = pos + want.size();
    }
  }

  return _separators.size() == 2;
}

size_t Headers::loadMore(const string_view view, Content &content, bool debug) {
  // keep loading if we haven't found the separators
  if (haveSeparators(view) == false) {
    static size_t count = 0;

    ++count;
    if ((count == 0) || (count % 20)) {
      fmt::print("{} separators not found count={}\n", fnName(), count);
    }

    return 1;
  }

  // NOTE:
  // this function does not clear previously found header key/val to avoid
  // the cost of recreating the header map on subsequent calls

  //  we've found the separaters, parse method and headers then check content length
  const size_t method_begin = 0;
  const size_t method_end = _separators.front();
  const std::string_view method_view = view.substr(method_begin, method_end);
  parseMethod(method_view);

  const size_t headers_begin = method_end + EOL.size();
  const size_t headers_end = _separators.back();
  const std::string_view hdr_block = view.substr(headers_begin, headers_end - headers_begin);
  parseHeaderBlock(hdr_block);

  // no content, we're done
  if (exists(header::type::ContentType) == false) {
    return 0;
  }

  // there's content, confirm we have it all
  const size_t content_begin = headers_end + SEP.size();
  const auto view_size = view.size();

  int32_t byte_diff = view_size - contentLength();

  // byte diff is negative indicating we need more data
  if (byte_diff < 0) {
    if (debug) {
      constexpr auto f = FMT_STRING("{} method={} path={} content_type={}"
                                    " have={} content_len={} diff={}\n");

      fmt::print(f, fnName(), method(), path(), contentType(), view_size, contentLength(),
                 byte_diff);
    }

    _more_bytes = std::abs(byte_diff);

    return _more_bytes;
  }

  // we have all the content, copy it
  content.clear();
  auto copy_begin = view.begin() + content_begin;
  auto copy_end = view.end();

  content.assign(copy_begin, copy_end);

  // if we've reached here then everything is as it should be
  if (debug) {
    constexpr auto f = FMT_STRING("{} complete method={} path={}"
                                  "content_type={} content_len={}\n");
    fmt::print(f, fnName(), method(), path(), contentType(), content.size());
  }

  return 0;
}

void Headers::parseHeaderBlock(const string_view &view) {
  // Example Headers
  //
  // Content-Type: application/octet-stream
  // Content-Length: 16
  // User-Agent: Music/1.2.2 (Macintosh; OS X 12.2.1) AppleWebKit/612.4.9.1.8
  // Client-Instance: BAFE421337BA1913

  auto sstream = std::istringstream(string(view));
  string line;

  while (std::getline(sstream, line)) {
    auto view = string_view(line);

    // eliminate those pesky \r at the end of the line
    if (view.back() == '\r') {
      view.remove_suffix(1);
    }

    // remember zero indexing!
    auto colon_pos = view.find_first_of(':');

    if (colon_pos != view.npos) {                     // we found a header line
      auto key = view.substr(0, colon_pos);           // bol to colon
      colon_pos++;                                    // skip space after the colon
      auto val = view.substr(++colon_pos, view.npos); // to eol

      if (false) { // debug
        auto constexpr f = FMT_STRING("{} key={} val={}\n");
        fmt::print(f, fnName(), key, val);
      }

      add(key, val);

    } else {
      constexpr auto f = FMT_STRING("WARN {} {}\n\tignored --> {} <-- ignored\n");
      fmt::print(f, fnName(), "colon not found"sv, line);
    }
  }
}

void Headers::parseMethod(csv &view) {
  // Example Method
  //
  // POST /fp-setup RTSP/1.0

  // NOTE: index 0 is entire string
  constexpr auto method_idx = 1;
  constexpr auto path_idx = 2;
  constexpr auto protocol_idx = 3;
  constexpr auto match_count = 4;

  std::smatch sm;
  static auto re_method = std::regex("([A-Z_]+) (.+) (RTSP.+)", re_syntax);

  const string line(view);

  std::regex_search(line, sm, re_method);

  if (sm.ready() && (sm.size() == match_count)) {
    _method = sm[method_idx].str();
    _path = sm[path_idx].str();
    _protocol = sm[protocol_idx].str();
  }
}

void Headers::dump() const {
  fmt::print("\n");

  fmt::print("HEADER DUMP method={} path={}\n", _method, _path);

  string header_list;
  auto where = std::back_inserter(header_list);

  list(where);
  fmt::print("{}\n", header_list);
}

} // namespace packet
} // namespace pierre
