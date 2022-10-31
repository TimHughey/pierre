// Pierre
// Copyright (C) 2021  Tim Hughey
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
// along with this program.  If not, see <https://www.gnu.org/licenses/>

#include "peaks/peak.hpp"
#include "base/helpers.hpp"
#include "peaks/peak_config.hpp"
#include "peaks/types.hpp"

namespace pierre {

// static Peak reference data
// PeakMagScaled Peak::mag_scaled(Peak::mag_base);

// explicit Peak::operator bool() const noexcept {
//   const auto ml = PeakConfig::mag_limits();
//   return (mag > ml.min()) && (mag < ml.max()) ? true : false;
// }

mag_min_max Peak::magScaleRange() noexcept { return PeakConfig::mag_limits().scaled(); }

bool Peak::useable() const noexcept {
  const auto ml = PeakConfig::mag_limits();

  return ((mag >= ml.min()) && (mag <= ml.max()));
}

} // namespace pierre