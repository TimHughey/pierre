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

#include "packet/content.hpp"

#include <algorithm>
#include <array>
#include <fmt/core.h>
#include <fmt/format.h>
#include <limits>
#include <list>
#include <map>
#include <regex>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace pierre {
namespace packet {

namespace header {

using csv = std::string_view;

struct type {
  static constexpr csv CSeq = "CSeq";
  static constexpr csv Server = "Server";
  static constexpr csv ContentSimple = "Content";
  static constexpr csv ContentType = "Content-Type";
  static constexpr csv ContentLength = "Content-Length";
  static constexpr csv Public = "Public";
  static constexpr csv DacpActiveRemote = "Active-Remote";
  static constexpr csv DacpID = "DACP-ID";
  static constexpr csv AppleProtocolVersion = "Apple-ProtocolVersion";
  static constexpr csv UserAgent = "User-Agent";
  static constexpr csv AppleHKP = "Apple-HKP";
  static constexpr csv XAppleClientName = "X-Apple-Client-Name";
  static constexpr csv XApplePD = "X-Apple-PD";
  static constexpr csv XAppleProtocolVersion = "X-Apple-ProtocolVersion";
  static constexpr csv XAppleHKP = "X-Apple-HKP";
  static constexpr csv XAppleET = "X-Apple-ET";
  static constexpr csv RtpInfo = "RTP-Info";
  static constexpr csv XAppleAbsoulteTime = "X-Apple-AbsoluteTime";
};

struct val {
  static constexpr auto OctetStream = "application/octet-stream";
  static constexpr auto AirPierre = "AirPierre/366.0";
  static constexpr auto AppleBinPlist = "application/x-apple-binary-plist";
  static constexpr auto TextParameters = "text/parameters";
  static constexpr auto ImagePng = "image/png";
  static constexpr auto ConnectionClosed = "close";
};

} // namespace header

namespace {
namespace ranges = std::ranges;
}

typedef fmt::basic_memory_buffer<char, 256> HeaderList;

class Headers {
public:
  using string = std::string;
  using string_view = std::string_view;

public:
  typedef const char *ccs;
  typedef const std::string_view csv;
  typedef std::map<csv, string> HeaderMap;
  typedef std::set<string> UnknownHeaders;

public:
  Headers() = default;

  void add(csv type, const string_view val);
  void add(csv type, size_t);

  size_t contentLength() const { return getValInt(header::type::ContentLength); }
  csv contentType() const { return getVal(header::type::ContentType); }
  void copy(const Headers &from, csv type);
  bool exists(csv type) const;
  bool isContentType(csv val) const;
  const string &getVal(csv want_type) const;
  size_t getValInt(csv want_type) const;

  // member functions that act on the entire container
  void clear();
  inline auto count() const { return _omap.size(); }
  void dump() const;

  void list(auto &where) const {
    ranges::for_each(_omap, [&](const auto &entry) {
      const auto &[type, val] = entry;

      fmt::format_to(where, "{}: {}\r\n", type, val);
    });
  }

  size_t loadMore(csv view, Content &content, bool debug = false);
  size_t moreBytes() const { return _more_bytes; }

  // preamble info
  csv method() const { return csv(_method); }
  csv path() const { return csv(_path); }
  csv protocol() const { return csv(_protocol); }

private:
  bool haveSeparators(const string_view &view);
  void parseHeaderBlock(const string_view &view);
  void parseMethod(const string_view &view);

  // misc
  const std::source_location
  here(std::source_location loc = std::source_location::current()) const {
    return loc;
  };

  const char *fnName(std::source_location loc = std::source_location::current()) const {
    return here(loc).function_name();
  }

private:
  HeaderMap _omap;
  UnknownHeaders _unknown;

  string _method;
  string _path;
  string _protocol;
  size_t _more_bytes = 0;

  bool _ok = false;
  std::vector<size_t> _separators;

  static constexpr auto re_syntax = std::regex_constants::ECMAScript;
  static constexpr string_view EOL{"\r\n"};
  static constexpr string_view SEP{"\r\n\r\n"};
  static constexpr auto THROW = std::numeric_limits<size_t>::max();
};

} // namespace packet
} // namespace pierre