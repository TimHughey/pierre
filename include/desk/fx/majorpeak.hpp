
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

#include "base/conf/token.hpp"
#include "base/elapsed.hpp"
#include "base/types.hpp"
#include "desk/color.hpp"
#include "desk/fx.hpp"
#include "desk/unit/all.hpp"
#include "frame/peaks.hpp"
#include "majorpeak/types.hpp"

#include <atomic>
#include <memory>
#include <optional>

namespace pierre {

namespace desk {

class MajorPeak : public FX {
public:
  MajorPeak(auto &executor) noexcept
      : FX(executor, fx::MAJOR_PEAK), tokc(module_id), base_color(Hsb{0, 100, 100}),
        main_last_peak(), fill_last_peak() {

    tokc.initiate();

    load_config();
  }

  void execute(const Peaks &peaks) noexcept override;

  void once() noexcept override;

private:
  using RefColors = std::vector<Color>;

private:
  void handle_el_wire(const Peaks &peaks);
  void handle_fill_pinspot(const Peaks &peaks);
  void handle_main_pinspot(const Peaks &peaks);

  bool load_config() noexcept;

  const Color make_color(const Peak &peak) noexcept { return make_color(peak, base_color); }
  const Color make_color(const Peak &peak, const Color &ref) noexcept;

  const Color &ref_color(size_t index) const noexcept;

private:
  // order dependent
  conf::token tokc;
  const Color base_color;
  Peak main_last_peak;
  Peak fill_last_peak;

  // order independent
  Elapsed color_elapsed;
  static RefColors ref_colors;

  // cached config
  major_peak::freq_limits_t freq_limits;
  major_peak::hue_cfg_map hue_cfg_map;
  mag_min_max mag_limits;
  major_peak::pspot_cfg_map pspot_cfg_map;

public:
  static constexpr auto module_id{"fx.majorpeak"sv};
};

} // namespace desk
} // namespace pierre
