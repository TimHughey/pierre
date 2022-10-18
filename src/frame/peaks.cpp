/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
    Copyright (C) 2020  Tim Hughey

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

#include "frame/peaks.hpp"
#include "base/logger.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <ranges>

namespace pierre {

bool Peaks::emplace(Magnitude m, Frequency f) noexcept {

  auto [element, inserted] = _peaks_map.emplace(m, Peak(m, f));

  if (!inserted) {
    INFO(module_id, "COLLISION", "mag={} freq={} collision, keeping {}\n", m, f, element->second);
  }

  return inserted;
}

peaks_t Peaks::sort() {
  ranges::sort(_peaks, [](Peak &lhs, Peak &rhs) { // order by magnitude
    return lhs.magnitude() > rhs.magnitude();
  });

  return shared_from_this();
}

} // namespace pierre
