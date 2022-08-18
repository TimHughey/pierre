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

#include <cmath>
#include <ranges>

namespace pierre {

shPeaks Peaks::sort() {
  ranges::sort(_peaks, [](const Peak &lhs, const Peak &rhs) { // reverse order by magnitude
    return lhs.magnitude() > rhs.magnitude();
  });

  return shared_from_this();
}

} // namespace pierre