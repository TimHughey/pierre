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

#include "base/content.hpp"
#include "base/typical.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <regex>
#include <set>

namespace {
namespace ranges = std::ranges;
}

namespace pierre {

struct hdr_type {
  static constexpr csv CSeq{"CSeq"};
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

struct hdr_val {
  static const string OctetStream;
  static const string AirPierre;
  static const string AppleBinPlist;
  static const string TextParameters;
  static const string ImagePng;
  static const string ConnectionClosed;
};

class Headers {
public:
  typedef std::map<csv, string> HeaderMap;
  typedef std::set<string> UnknownHeaders;

public:
  Headers() = default;

  void add(csv type, csv val); // must be in .cpp
  void add(csv type, size_t val) { add(type, fmt::format("{}", val)); }
  size_t contentLength() const { return getValInt(hdr_type::ContentLength); }
  csv contentType() const { return getVal(hdr_type::ContentType); }
  void copy(const Headers &from, csv type);
  bool exists(csv type) const { return map.contains(type); }
  bool contentTypeEquals(csv want_val) const {
    const auto &search = map.find(hdr_type::ContentType);
    return (search != map.end()) && (want_val == search->second) ? true : false;
  }

  const string_view getVal(csv want_type) const;
  size_t getValInt(csv want_type) const;

  // member functions that act on the entire container
  void clear();
  auto count() const { return map.size(); }
  void dump() const;

  void list(auto &where) const {
    ranges::for_each(map, [&](const auto &entry) {
      fmt::format_to(where, "{}: {}\r\n", entry.first, entry.second);
    });
  }

  size_t loadMore(csv view, Content &content);
  size_t moreBytes() const { return _more_bytes; }

  static bool valEquals(csv v1, csv v2) { return v1 == v2; }

  // preamble info
  csv method() const { return csv(_method); }
  csv path() const { return csv(_path); }
  csv protocol() const { return csv(_protocol); }

private:
  bool haveSeparators(const string_view &view);
  void parseHeaderBlock(const string_view &view);
  void parseMethod(const string_view &view);

private:
  HeaderMap map;
  UnknownHeaders unknown;

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
  static constexpr csv moduleId{"HEADERS"};
};

} // namespace pierre
