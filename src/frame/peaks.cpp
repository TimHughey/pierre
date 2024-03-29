// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "peaks.hpp"
#include "lcs/config.hpp"
#include "peaks/peak_config.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <ranges>

namespace pierre {

bool Peaks::emplace(Magnitude m, Frequency f, CHANNEL channel) noexcept {
  auto rc = false;

  const auto ml = PeakConfig::mag_limits();

  auto &map = peaks_map[channel];

  if ((m >= ml.min()) && (m <= ml.max())) {
    auto node = map.try_emplace(m, f, m);

    rc = node.second; // was the peak inserted?
  }

  return rc;
}

} // namespace pierre
