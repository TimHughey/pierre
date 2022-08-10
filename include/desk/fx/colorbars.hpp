/*
    lightdesk/fx/colorbars.hpp -- LightDesk Effect PinSpot with White Fade
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "desk/fx.hpp"
#include "fader/toblack.hpp"
#include "frame/peaks.hpp"

namespace pierre {
namespace fx {

typedef fader::ToBlack<fader::Circular> Fader;

class ColorBars : public FX {
public:
  ColorBars() = default;

  void execute(shPeaks peaks) override;
  csv name() const override { return fx::COLOR_BARS; }

private:
  uint8_t bar_count = 10;

  static constexpr Millis BAR_MS{300};
};

} // namespace fx
} // namespace pierre
