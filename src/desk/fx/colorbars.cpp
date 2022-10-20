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

#include "desk/fx/colorbars.hpp"
#include "base/color.hpp"
#include "desk/desk.hpp"
#include "desk/unit/pinspot.hpp"

#include <chrono>

namespace pierre {
namespace fx {

void ColorBars::execute([[maybe_unused]] peaks_t peaks) {
  Fader::Opts opts{.origin = Color(), .duration = pet::as<Millis>(BAR_MS)};

  shPinSpot main = units->derive<PinSpot>(unit::MAIN_SPOT);
  shPinSpot fill = units->derive<PinSpot>(unit::FILL_SPOT);

  if (main->isFading() || fill->isFading()) {
    // this is a no op while the PinSpots are fading
    return;
  }

  // which pinspot should this call to execute() act upon?
  auto pinspot = ((bar_count % 2) == 0) ? main : fill;

  switch (bar_count) {
  case 1:
    finished = true;
    break;

  case 2:
    main->colorNow(Color::black());
    fill->colorNow(Color::black());
    break;

  case 3:
  case 4:
    opts.origin = Color::full();
    break;

  case 5:
  case 6:
    opts.origin = Color(0x0000ff);
    break;

  case 7:
  case 8:
    opts.origin = Color(0x00ff00);
    break;

  case 9:
  case 10:
    opts.origin = Color(0xff0000);
    break;
  }

  if (bar_count > 2) {
    pinspot->activate<Fader>(opts);
  }

  --bar_count;
}

} // namespace fx
} // namespace pierre
