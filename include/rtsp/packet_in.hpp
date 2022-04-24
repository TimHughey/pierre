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
#include <string_view>
#include <vector>

namespace pierre {
namespace rtsp {

class PacketIn : public std::vector<uint8_t> {
public:
  typedef const std::string_view csv;
  typedef const char *ccs;

public:
  ccs raw() const { return (ccs)data(); }
  const csv view() const { return csv(raw(), size()); }
};

} // namespace rtsp
} // namespace pierre