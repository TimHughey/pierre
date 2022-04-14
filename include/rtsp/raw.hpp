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
#include <tuple>

namespace pierre {
namespace rtsp {

typedef std::tuple<uint8_t *, size_t> PartialRaw;

class Raw : public std::array<char, 2048> {
public:
  Raw() { fill(0x00); };

  operator uint8_t *() { return reinterpret_cast<uint8_t *>(data()); }

  const PartialRaw partial(const size_t offset) {
    auto _begin = begin() + offset;
    size_t _size = std::distance(_begin, end());

    return PartialRaw(reinterpret_cast<uint8_t *>(_begin), _size);
  }
};

} // namespace rtsp
} // namespace pierre