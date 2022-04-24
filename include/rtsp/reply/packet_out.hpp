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

#include <cstddef>
#include <fmt/format.h>
#include <memory>
#include <string_view>
#include <vector>

namespace pierre {
namespace rtsp {
class PacketOut : public std::vector<uint8_t> {
  static constexpr size_t reserve_default = 1024;

public:
  PacketOut() { reserve(reserve_default); };

  void dump() const {
    fmt::print("PACKET OUT DUMP BEGIN bytes={}\n", size());
    fmt::print("{}\n", view());
    fmt::print("PACKET OUT DUMP END\n");
  }

  std::string_view view() const {
    const char *view_data = (const char *)data();
    size_t view_len = size();

    return std::string_view(view_data, view_len);
  }

  void reset(size_t reserve_bytes = reserve_default) {
    clear();
    reserve(reserve_bytes);
  }
};

} // namespace rtsp
} // namespace pierre