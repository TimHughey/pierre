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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#include "conn_info/conn_info.hpp"

#include <chrono>
#include <fmt/format.h>
#include <memory>
#include <string_view>

namespace pierre {
namespace airplay {

namespace shared {
std::optional<shConnInfo> __conn_info;
std::optional<shConnInfo> &connInfo() { return __conn_info; }
} // namespace shared

void ConnInfo::save(const StreamData &data) {
  stream_info = data;
  SharedKey::save(data.key);
}

} // namespace airplay
} // namespace pierre