
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

#include "base/color.hpp"
#include "base/elapsed.hpp"
#include "base/types.hpp"
#include "config/types.hpp"
#include "desk/fx.hpp"
#include "frame/peaks.hpp"
#include "majorpeak/types.hpp"

#include <optional>

namespace pierre {
namespace fx {

class MajorPeak : public FX {
public:
  MajorPeak(pierre::desk::Stats &stats) noexcept;

  void execute(peaks_t peaks) override;
  csv name() const override { return fx::MAJOR_PEAK; }

  void once() override;

private:
  typedef std::vector<Color> ReferenceColors;

private:
  void handle_el_wire(peaks_t peaks);
  void handle_led_forest(peaks_t peaks);
  void handle_fill_pinspot(peaks_t peaks);
  void handle_main_pinspot(peaks_t peaks);

  void load_config() noexcept;

  const Color make_color(const Peak &peak) noexcept { return make_color(peak, base_color); }
  const Color make_color(const Peak &peak, const Color &ref) noexcept;
  const Color &ref_color(size_t index) const noexcept;

private:
  // order dependent
  const Color base_color;
  pierre::desk::Stats &stats;
  Peak _main_last_peak;
  Peak _fill_last_peak;

  // order independent
  Elapsed color_elapsed;
  static ReferenceColors _ref_colors;

  // cached config
  cfg_future _cfg_changed;
  major_peak::freq_limits_t _freq_limits;
  major_peak::hue_cfg_map _hue_cfg_map;
  mag_min_max _mag_limits;
  major_peak::pspot_cfg_map _pspot_cfg_map;

public:
  static constexpr csv module_id{"MAJOR_PEAK"};
};

} // namespace fx
} // namespace pierre
