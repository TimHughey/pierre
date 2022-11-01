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

    constexpr const min_max<Frequency> hue_minmax() const noexcept {
      return min_max<Frequency>(min(), max());
    }
  };

  static hard_soft<Frequency> freq_limits() noexcept {
    auto freqs = Config().at_path("fx.majorpeak.frequencies"sv);

    return hard_soft<Frequency>(freqs.at_path("hard.floor"sv).value_or(40.0),
                                freqs.at_path("hard.ceiling"sv).value_or(11500.0),
                                freqs.at_path("soft.floor"sv).value_or(110.0),
                                freqs.at_path("soft.ceiling"sv).value_or(10000.0));
  }

  static hue_cfg make_colors(csv cat) noexcept {
    const string full_path = fmt::format("fx.majorpeak.makecolors.{}", cat);

    auto cc = Config().at_path(full_path);

    return hue_cfg{.hue = {.min = cc.at_path("hue.min"sv).value_or(0.0),
                           .max = cc.at_path("hue.max"sv).value_or(0.0),
                           .step = cc.at_path("hue.step"sv).value_or(0.001)},
                   .brightness = {.max = cc.at_path("bri.max"sv).value_or(0.0),
                                  .mag_scaled = cc.at_path("hue.mag_scaled"sv).value_or(true)}};
  }

  static mag_min_max mag_limits() noexcept {
    auto mags = Config().at_path("fx.majorpeak.magnitudes"sv);

    return mag_min_max(mags.at_path("floor").value_or(2.009),
                       mags.at_path("ceiling").value_or(64.0));
  }
};

} // namespace fx

} // namespace pierre