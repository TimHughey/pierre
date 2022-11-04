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

#include "peaks/peak_config.hpp"
#include "config/config.hpp"
#include "peaks/types.hpp"

namespace pierre {

// static member data

mag_min_max PeakConfig::mag_limits() noexcept {

  // defaults
  static const Magnitude floor(2.1);
  static const Magnitude ceiling(32.0);

  static const toml::path path{"frame.peaks.magnitudes"sv};

  if (const auto mags = Config().table()[path]; mags) {
    return mag_min_max(mags["floor"sv].value_or<double>(floor), //
                       mags["ceiling"sv].value_or<double>(ceiling));
  }

  return mag_min_max(floor, ceiling);
}

} // namespace pierre