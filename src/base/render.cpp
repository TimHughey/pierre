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

#include "render.hpp"
#include "logger.hpp"

namespace pierre {

namespace shared {
Render render;
}

bool Render::enabled(bool wait) noexcept {
  if (wait && !shared::render.mode()) {
    if (shared::render.mode() == false) {
      shared::render.flag.wait(false);
    }
  }

  return shared::render.mode();
}

csv Render::inspect() noexcept {
  return shared::render.mode() ? csv{"RENDERING"} : csv{"NOT_RENDERING"};
}

void Render::set(uint64_t v) noexcept {
  bool next = v & 0x01;
  bool prev = false;

  if (next) {
    prev = shared::render.flag.test_and_set();
  } else {
    shared::render.flag.clear();
  }

  if (prev != next) {
    INFO(module_id, "SET", "{} => {}\n", prev, next);
  }

  shared::render.flag.notify_all();
}

} // namespace pierre