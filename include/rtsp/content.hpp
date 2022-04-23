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

#include <cstdint>
#include <string>
#include <vector>

namespace pierre {
namespace rtsp {

class Content : public std::vector<uint8_t> {
public:
  typedef const char *ccs;

public:
  void dump() const;
  void storeContentType(const std::string &type) { _type = type; }
  const std::string_view toStringView() const;
  const std::string &type() const { return _type; }

private:
  bool printable() const;

private:
  std::string _type;
};

} // namespace rtsp
} // namespace pierre