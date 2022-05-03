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
#include <array>
#include <exception>
#include <fmt/format.h>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "packet/content.hpp"
#include "packet/headers.hpp"
#include "packet/resp_code.hpp"

namespace pierre {
namespace packet {

using namespace std::literals;
using std::regex;
using std::string, std::string_view, std::unordered_map, std::back_inserter;
using enum Headers::Type2;
using enum Headers::Val2;

// map enums for header type and val to actual text
static unordered_map<Headers::Type2, const char *> _header_types // next line
    {{None, "None"},
     {CSeq, "CSeq"},
     {Server, "Server"},
     {ContentSimple, "Content"},
     {ContentType, "Content-Type"},
     {ContentLength, "Content-Length"},
     {Public, "Public"},
     {DacpActiveRemote, "Active-Remote"},
     {DacpID, "DACP-ID"},
     {AppleProtocolVersion, "Apple-ProtocolVersion"},
     {UserAgent, "User-Agent"},
     {AppleHKP, "Apple-HKP"},
     {XAppleClientName, "X-Apple-Client-Name"},
     {XApplePD, "X-Apple-PD"},
     {XAppleProtocolVersion, "X-Apple-ProtocolVersion"},
     {XAppleHKP, "X-Apple-HKP"},
     {XAppleET, "X-Apple-ET"},
     {RtpInfo, "RTP-Info"},
     {XAppleAbsoulteTime, "X-Apple-AbsoluteTime"}};

static unordered_map<Headers::Val2, const char *> _header_vals // next line
    {{OctetStream, "application/octet-stream"},
     {AirPierre, "AirPierre/366.0"},
     {AppleBinPlist, "application/x-apple-binary-plist"},
     {TextParameters, "text/parameters"},
     {ImagePng, "image/png"},
     {ConnectionClosed, "close"}};

// add to headers matching how it is stored in the map
void Headers::add(Type2 type, const string &val) {
  const auto [__it, inserted] = _map.try_emplace(type, val);

  if (inserted) {
    _order.emplace_back(type);
  }
}

// support string_view type and val
void Headers::add(const string_view type_sv, const string_view val) {
  add(toType(type_sv), string(val));
}

// support const char *
void Headers::add(Headers::ccs type_ccs, Headers::ccs val_ccs) {
  add(string_view(type_ccs), string_view(val_ccs));
}

// support type and val enumerations
void Headers::add(Type2 type, Val2 val) { add(type, stringFrom(val)); }

// support typp enumeration and a size_t numeric
void Headers::add(Type2 type, size_t val) { add(type, fmt::format("{}", val)); }

void Headers::copy(const Headers &from, Type2 type) { add(type, from.getVal(type)); }

void Headers::clear() {
  _map.clear();
  _order.clear();
  _separators.clear();

  _method.clear();
  _path.clear();
  _protocol.clear();

  _more_bytes = 0;
  _ok = false;
}

bool Headers::exists(Type2 type) const { return _map.contains(type); }

bool Headers::isContentType(Val2 val) const {
  auto rc = false;

  if (exists(ContentType)) {
    const auto want_val_str = stringFrom(val);
    const auto val_str = getVal(ContentType);

    rc = want_val_str.compare(val_str) == 0;
  }

  return rc;
}

const string &Headers::getVal(Type2 want_type, Type2 default_type) const {
  if (exists(want_type)) {
    return _map.at(want_type);
  }

  if (default_type != Unknown) {
    return getVal(default_type);
  }

  fmt::print("{} not in map type={}\n", fnName(), toString(want_type));
  throw(std::runtime_error("header type not found"));
}

size_t Headers::getValInt(Type2 want_type, size_t def_val) const {
  if (exists(want_type)) {
    const auto val = getVal(want_type);

    return static_cast<size_t>(std::atoi(val.c_str()));
  }

  if (def_val != THROW) {
    return def_val;
  }

  throw(std::runtime_error("header type not found"));
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

const HeaderList Headers::list() const {
  HeaderList list_text;

  auto where = back_inserter(list_text);

  for (const auto type : _order) {
    const auto type_str = toString(type);
    const auto val_str = getVal(type);

    fmt::format_to(where, "{}: {}\r\n", type_str, val_str);
  }

  return list_text;
}

size_t Headers::loadMore(const string_view view, Content &content, bool debug) {
  // keep loading if we haven't found the separators
  if (haveSeparators(view) == false) {
    fmt::print("{} separators not found separators={}\n", fnName(), _separators.size());
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
  if (exists(ContentType) == false) {
    return 0;
  }

  // there's content, confirm we have it all
  const size_t content_begin = headers_end + SEP.size();
  // const size_t content_end = content_begin + getValInt(ContentLength);

  const auto view_size = view.size();
  // _more_bytes = content_end - view_size; // save amount of bytes we need

  int32_t byte_diff = view_size - getValInt(ContentLength);

  // byte diff is negative indicating we need more data
  if (byte_diff < 0) {
    if (debug) {
      auto content_type = getVal(ContentType, None);

      fmt::print("{} method={} path={} content_type={} have={} content_len={} diff={}\n", fnName(),
                 method(), path(), content_type, view_size, getValInt(ContentLength), byte_diff);
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
    fmt::print("{} complete method={} path={} content_type={} content_len={}\n", fnName(),
               method(), path(), getVal(ContentType), content.size());
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

  // for (string line; std::getline(sstream, line);) {
  //   auto trimmed = string_view(line);
  //   trimmed.remove_suffix(1);
  //   auto f = FMT_STRING("{} '{}'\n");
  //   fmt::print(f, fnName(), trimmed);
  // }

  auto blk_begin = view.begin();
  auto blk_end = view.end();

  constexpr auto CR = '\r';
  auto isCR = [](char c) { return c == CR; };

  for (auto bol = blk_begin; bol < blk_end;) {
    // find the end of CR
    auto eol = std::find_if(bol, blk_end, isCR);

    // end of the block, break out to simplify the following logic
    if (eol >= blk_end) {
      break;
    }

    // we found a line, turn it into a string for Regex
    const auto line = string(bol, eol);
    bol = ++eol; // setup the next bol

    // NOTE: index 0 is entire string
    constexpr auto key_idx = 1;
    constexpr auto val_idx = 2;
    constexpr auto match_count = 3;

    std::smatch sm;
    // reminder...  (.+) will not match \r or \n
    auto re_method = regex{"([\\w]+[\\w-]*): (.+)", re_syntax};
    regex_search(line, sm, re_method);

    // if the regex is ready and the num matches meet expectations
    if (sm.ready() && (sm.size() == match_count)) {
      const auto &key = sm[key_idx].str();
      const auto &val = sm[val_idx].str();

      try {
        add(key, val);
      } catch (const std::exception &e) {
        fmt::print("{}\n{}\n", view, e.what());
        throw(std::runtime_error("unknown header type"));
      }
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

  const auto list_text = fmt::to_string(list());
  fmt::print("{}", list_text);
}

const string Headers::stringFrom(Type2 type) const { return string(_header_types[type]); }
const string Headers::stringFrom(Val2 val) const { return string(_header_vals[val]); }

Headers::Type2 Headers::toType(csv type_view) const {
  for (const auto &kv : _header_types) {
    const auto &[key_type, key_str] = kv;

    if (type_view.compare(key_str) == 0) {
      return key_type;
    }
  }

  fmt::print("FAILED: converting type_view={}\n", type_view);
  throw(std::runtime_error("toType failed"));
}

const string_view Headers::toString(Headers::Type2 type) const { return _header_types.at(type); }

} // namespace packet
} // namespace pierre
