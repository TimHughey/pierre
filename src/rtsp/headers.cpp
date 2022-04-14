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

#include <exception>
#include <fmt/format.h>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>

#include "rtsp/headers.hpp"

namespace pierre {
namespace rtsp {

using std::string, std::string_view, std::unordered_map, std::back_inserter;
using enum Headers::Type2;
using enum Headers::Val2;

// map enums for header type and val to actual text
static unordered_map<Headers::Type2, const char *> _header_types // next line
    {{CSeq, "CSeq"},
     {Server, "Server"},
     {ContentType, "Content-Type"},
     {ContentLength, "Content-Length"},
     {Public, "Public"}};

static unordered_map<Headers::Val2, const char *> _header_vals // next line
    {{OctetStream, "application/octet-stream"},
     {AirPierre, "AirPierre/366.0"},
     {AppleBinPlist, "application/x-apple-binary-plist"}};

//
// BEGIN MEMBER FUNCTIONS
//

void Headers::headerAdd(HeaderType type, HeaderVal val) {
  // ensure all rvalues (tuple is unnamed)
  _headers.emplace_back(Entry(type, val));
}

void Headers::headerAdd(Type2 type, Val2 val) {
  auto type_string = stringFrom(type);
  auto val_string = stringFrom(val);

  _headers.emplace_back(type_string, val_string);
}

void Headers::headerAdd(Type2 type, size_t val) {
  auto type_string = stringFrom(type);
  auto val_string = fmt::format("{}", val);

  headerAdd(type_string, val_string);
}

void Headers::headerAdd(Type2 type, HeaderVal val) {
  auto type_string = stringFrom(type);

  _headers.emplace_back(type_string, val);
}

void Headers::headerCopy(const Headers &from, Type2 type) {
  const auto [type_str, val_str] = from.headerFetch(type);

  headerAdd(type_str, val_str);
}

const Headers::Entry &Headers::headerFetch(Type2 want_type) const {
  auto want_string = stringFrom(want_type);

  for (const auto &entry : _headers) {
    auto &[type, value] = entry;

    if (want_string.compare(type) == 0) {
      return entry;
    }
  }

  throw std::runtime_error("header type not found");
}

const HeaderVal &Headers::headerGetValue(Type2 want_type) const {
  auto &[type, value] = headerFetch(want_type);

  return value;
}

void Headers::headersDump() const {
  fmt::print("\n");

  for (const auto &[key, value] : _headers) {
    fmt::print("{: >25} {}\n", key, value);
  }
}

const HeaderList Headers::headersList() const {
  HeaderList list_text;

  auto where_it = back_inserter(list_text);

  for (const auto &entry : _headers) {
    auto &[type, val] = entry;

    fmt::format_to(where_it, "{}: {}\r\n", type, val);
  }

  return list_text;
}

const string Headers::stringFrom(Type2 type) const { return string(_header_types[type]); }
const string Headers::stringFrom(Val2 val) const { return string(_header_vals[val]); }

} // namespace rtsp
} // namespace pierre
