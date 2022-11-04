
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
#include "desk/fx.hpp"
#include "frame/peaks.hpp"

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
  void handleElWire(peaks_t peaks);
  void handleLedForest(peaks_t peaks);
  void handleFillPinspot(peaks_t peaks);
  void handleMainPinspot(peaks_t peaks);

  const Color make_color(const Peak &peak) const noexcept { return make_color(peak, base_color); }
  const Color make_color(const Peak &peak, const Color &ref) const noexcept;
  Color &refColor(size_t index) const;

private:
  // order dependent
  const Color base_color;
  pierre::desk::Stats &stats;
  Peak _main_last_peak;
  Peak _fill_last_peak;

  // order independent
  Elapsed color_elapsed;
  static ReferenceColors _ref_colors;

public:
  static constexpr csv module_id{"MAJOR_PEAK"};
};

} // namespace fx
} // namespace pierre
