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
#include <map>

namespace pierre {
namespace fx {
namespace major_peak {

using freq_limits_t = hard_soft_limit<Frequency>;

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

struct pspot_cfg {
  string name;           // common for all pinspots
  string type;           // type of config (fill or main)
  Nanos fade_max{0};     // common for all pinspots
  Frequency freq_min{0}; // main pinspot
  Frequency freq_max{0}; // fill pinspot

  struct {
    Frequency freq{0};
    double bri_min{0};
  } when_less_than{0}; // main or fill

  struct {
    Frequency freq{0};
    double bri_min{0};
    struct {
      double bri_min{0};
    } when_higher_freq{0};
  } when_greater; // fill

  struct {
    double bri_min{0};
    struct {
      double bri_min{0};
    } when_freq_greater;
  } when_fading; // main

  pspot_cfg() = default;
  pspot_cfg(csv name, csv type, const Nanos &fade_max, const Frequency freq_min,
            const Frequency freq_max) noexcept
      : name(name), type(type), fade_max(fade_max), freq_min(freq_min), freq_max(freq_max) {}
};

using pspot_cfg_map = std::map<string, pspot_cfg>;
using hue_cfg_map = std::map<string, hue_cfg>;

} // namespace major_peak
} // namespace fx
} // namespace pierre