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
#include <fmt/format.h>
#include <list>
#include <regex>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "rtsp/content.hpp"
#include "rtsp/packet_in.hpp"

namespace pierre {
namespace rtsp {

typedef fmt::basic_memory_buffer<char, 256> HeaderList;

enum RespCode : uint16_t {
  OK = 200,
  AuthRequired = 470,
  BadRequest = 400,
  Unauthorized = 403,
  Unavailable = 451,
  InternalServerError = 500,
  NotImplemented = 501
};

class Headers {
public:
  enum Type2 : uint8_t {
    CSeq = 1,
    Server,
    ContentType,
    ContentLength,
    Public,
    DacpActiveRemote,
    DacpID,
    AppleProtocolVersion,
    UserAgent,
    AppleHKP,
    XAppleClientName,
    XApplePD,
    XAppleProtocolVersion,
    XAppleHKP,
    XAppleET

  };

  enum Val2 : uint8_t { OctetStream = 1, AirPierre, AppleBinPlist, TextParameters };

public:
  using string = std::string;
  using string_view = std::string_view;

public:
  typedef std::unordered_map<Type2, string> HeaderMap;
  typedef std::list<Type2> HeaderOrder;
  typedef const char *ccs;

public:
  Headers() = default;

  // void add(const string_view &type, const string_view &val);
  void add(Type2 type, const string &val);
  void add(const string_view type, const string_view val);
  void add(ccs type, ccs val);
  void add(Type2 type, Val2 val);
  void add(Type2 type, size_t val);

  size_t contentLength() const { return getValInt(ContentLength); }
  void copy(const Headers &from, Type2 type);
  bool exists(Type2 type) const;
  const string &getVal(Type2 want_type) const;
  size_t getValInt(Type2 want_type) const;

  // member functions that act on the entire container
  inline void clear() {
    _map.clear();
    _order.clear();
    _separators.clear();
  }
  inline auto count() const { return _map.size(); }
  void dump() const;

  const HeaderList list() const;
  bool loadMore(const string_view view, Content &content);

  // preamble info
  const string_view method() const { return string_view(_method); }
  const string_view path() const { return string_view(_path); }
  const string_view protocol() const { return string_view(_protocol); }

private:
  bool haveSeparators(const string_view &view);
  void parseHeaderBlock(const string_view &view);
  void parseMethod(const string_view &view);

  const string stringFrom(Type2 type) const;
  const string stringFrom(Val2 val) const;
  Type2 toType(string_view type_sv) const;
  const string_view toString(Type2 type) const;

  // misc
  const std::source_location
  here(std::source_location loc = std::source_location::current()) const {
    return loc;
  };

  const char *fnName(std::source_location loc = std::source_location::current()) const {
    return here(loc).function_name();
  }

private:
  HeaderMap _map;
  HeaderOrder _order;

  string _method;
  string _path;
  string _protocol;
  // size_t _content_len = 0;

  bool _ok = false;
  std::vector<size_t> _separators;

  static constexpr auto re_syntax = std::regex_constants::ECMAScript;
  static constexpr string_view EOL{"\r\n"};
  static constexpr string_view SEP{"\r\n\r\n"};
};

} // namespace rtsp
} // namespace pierre