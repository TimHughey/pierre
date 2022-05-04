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

#include <chrono>
#include <fmt/format.h>
#include <memory>
#include <string_view>

#include "decouple/conn_info.hpp"

namespace pierre {

shConnInfo ConnInfo::__inst__;

ConnInfo::ConnInfo(){};

void ConnInfo::saveSessionKey(csr key) { session_key.assign(key.begin(), key.end()); }

void ConnInfo::reset(const src_loc loc) {
  if (__inst__.use_count() > 1) {
    constexpr auto f = FMT_STRING("WARN {} ConnInfo use_count={}\n");
    fmt::print(f, fnName(loc), __inst__.use_count());
  }

  __inst__.reset();
}

shConnInfo ConnInfo::inst() {
  if (__inst__.use_count() == 0) {
    __inst__ = shConnInfo(new ConnInfo());
  }

  return __inst__->shared_from_this();
}

} // namespace pierre