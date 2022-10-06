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

#include "desk/fx/leave.hpp"
#include "desk/desk.hpp"
#include "desk/unit/all.hpp"

namespace pierre {
namespace fx {

Leave::Leave(const float hue_step, const float brightness)
    : hue_step(hue_step), brightness(brightness), next_color({.hue = 0, .sat = 100, .bri = brightness}) {}

void Leave::execute([[maybe_unused]] peaks_t peaks) {
  if (next_brightness < 50.0) {
    next_brightness++;
    next_color.setBrightness(next_brightness);
  }

  units->derive<PinSpot>(unit::MAIN_SPOT)->colorNow(next_color);
  units->derive<PinSpot>(unit::FILL_SPOT)->colorNow(next_color);

  if (next_brightness >= 50.0) {
    next_color.rotateHue(hue_step);
  }
}

void Leave::once() { units->leave(); }

} // namespace fx
} // namespace pierre
