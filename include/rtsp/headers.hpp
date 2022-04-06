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

#include <fmt/format.h>
#include <list>
#include <string>
#include <tuple>

namespace pierre {
namespace rtsp {

using std::string, std::string_view, std::tuple;

typedef const std::string HeaderType;
typedef const std::string HeaderVal;
typedef fmt::basic_memory_buffer<char, 50> HeaderList;

class Headers {
public:
  typedef tuple<HeaderType, HeaderVal> Entry;

  enum Type2 : uint8_t { CSeq = 1, Server, ContentType, ContentLength, Public };
  enum Val2 : uint8_t { OctetStream = 1, AirPierre };

public:
  Headers() = default;

  // void add(const string_view &type, const string_view &val);
  void headerAdd(Type2 type, Val2 val);
  void headerAdd(Type2 type, size_t val);
  void headerAdd(Type2 type, HeaderVal val);
  void headerAdd(HeaderType type, HeaderVal val);
  void headerCopy(const Headers &from, Type2 type);
  const Entry &headerFetch(Type2 want_type) const;
  const HeaderVal &headerGetValue(Type2 want_type) const;

  // member functions that act on the entire container
  void headersDump() const;
  const HeaderList headersList() const;

private:
  const string stringFrom(Type2 type) const;
  const string stringFrom(Val2 val) const;

private:
  std::list<Entry> _headers;
};

} // namespace rtsp
} // namespace pierre