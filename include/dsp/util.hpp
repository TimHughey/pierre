/*
    Pierre
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

#pragma once

#include "base/types.hpp"

#include <cmath>

namespace pierre {
namespace peak {

constexpr Scaled scaleVal(Unscaled val) {
  // log10 of 0 is undefined
  if (val <= 0.0f) {
    return 0.0f;
  }

  return 10.0f * std::log10(val);
}

} // namespace peak
} // namespace pierre