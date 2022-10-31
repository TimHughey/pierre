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

#include "base/hard_soft.hpp"
#include "base/minmax.hpp"
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

  static hard_soft<Frequency> freq_limits() noexcept {
    auto freqs = Config().at_path("fx.majorpeak.frequencies"sv);

    return hard_soft<Frequency>(freqs.at_path("hard.floor"sv).value_or(40.0),
                                freqs.at_path("hard.ceiling"sv).value_or(11500.0),
                                freqs.at_path("soft.floor"sv).value_or(110.0),
                                freqs.at_path("soft.ceiling"sv).value_or(10000.0));
  }
};

} // namespace fx

} // namespace pierre