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

#include "base/hard_soft_limit.hpp"
#include "base/min_max_pair.hpp"
#include "base/types.hpp"
#include "frame/peaks.hpp"

#include <fmt/format.h>

namespace pierre {
namespace fx {

struct major_peak_config {

  struct hue_cfg {
    struct {
      Frequency min;
      Frequency max;
      double step;
    } hue;

    struct {
      double max;
      bool mag_scaled;
    } brightness;

    constexpr const Frequency min() const noexcept { return hue.min * (1.0 / hue.step); }
    constexpr const Frequency max() const noexcept { return hue.max * (1.0 / hue.step); }

    constexpr const min_max_pair<Frequency> hue_minmax() const noexcept {
      return min_max_pair<Frequency>(min(), max());
    }
  };

  static hard_soft_limit<Frequency> freq_limits() noexcept;
  static hue_cfg make_colors(csv cat) noexcept;
  static mag_min_max mag_limits() noexcept;
};

} // namespace fx

} // namespace pierre