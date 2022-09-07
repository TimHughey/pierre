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

#include "threads.hpp"

namespace pierre {

void name_thread(csv name) {
  const auto handle = pthread_self();

  __LOGX(LCOL01 " {:<16} handle={:#x}\n", "BASE", "NAME THREAD", name, handle);

  pthread_setname_np(handle, name.data());
}

void name_thread(csv name, int num) { name_thread(fmt::format("{} {}", name, num)); }
} // namespace pierre
