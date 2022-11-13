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

#include "base/types.hpp"

#include <atomic>

namespace pierre {

class Render {
public:
  Render() = default;

  static bool enabled() noexcept;
  static csv inspect() noexcept;
  static void set(uint64_t v) noexcept;

private:
  bool mode() const noexcept { return flag.test(); }

private:
  std::atomic_flag flag;

public:
  static constexpr csv module_id{"RENDER"};
};

} // namespace pierre
