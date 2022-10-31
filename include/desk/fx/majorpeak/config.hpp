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

#pragma once

#include "base/types.hpp"
#include "config/config.hpp"
#include "frame/peaks.hpp"

namespace pierre {
namespace fx {

struct major_peak_config {

  static mag_min_max mag_limits() noexcept {
    auto mags = Config().at_path("fx.majorpeak.magnitudes"sv);

    return mag_min_max(mags.at_path("floor").value_or(2.009),
                       mags.at_path("ceiling").value_or(64.0));
  }
};

} // namespace fx

} // namespace pierre