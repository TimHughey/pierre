/* Pierre - Custom Light Show for Wiss Landing
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

    https://www.wisslanding.co */

#pragma once

#include "base/logger.hpp"
#include "base/types.hpp"

#include <atomic>

namespace pierre {

class Render {
public:
  Render() = default;

  static bool enabled() noexcept { return flag.load(); }

  static csv inspect() noexcept { return mode() ? csv{"RENDERING"} : csv{"NOT_RENDERING"}; }

  static void set(bool next) noexcept {
    bool current = flag.load();

    if (current != next) {
      INFO(module_id, "SET", "{} => {}\n", current, next);
      flag.store(next);
      flag.notify_all();
    }
  }

private:
  static bool mode() noexcept { return flag.load(); }

private:
  static std::atomic_bool flag;

public:
  static constexpr csv module_id{"RENDER"};
};

} // namespace pierre
