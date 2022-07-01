/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#include "base/color.hpp"
#include "base/typical.hpp"
#include "desk/fx.hpp"
#include "dsp/peaks.hpp"

namespace pierre {
namespace fx {

class Leave : public FX {
public:
  Leave(const float hue_step = 0.25f, const float brightness = 100.0f);
  ~Leave() = default;

  void execute(shPeaks peaks) override;
  csv name() const override { return fx::LEAVING; };

  void once() override;

private:
  float hue_step = 0.25f;
  float brightness = 100.0f;

  float next_brightness = 0;
  Color next_color;
};

} // namespace fx
} // namespace pierre
