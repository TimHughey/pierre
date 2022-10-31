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
#include "config/config.hpp"
#include "peaks/peak_config.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <ranges>

namespace pierre {

Peaks::Peaks() noexcept {}

bool Peaks::emplace(Magnitude m, Frequency f) noexcept {
  auto rc = false;

  const auto ml = PeakConfig::mag_limits();

  if ((m >= ml.min()) && (m <= ml.max())) {
    auto node = peaks_map.try_emplace(m, f, m);

    rc = node.second; // was the peak inserted?

    if (!rc) {
      const auto &peak = node.first->second;
      INFO(module_id, "COLLISION", "keeping {} alt freq={:0.2f}\n", peak, f);
    }
  }

  return rc;
}

} // namespace pierre
