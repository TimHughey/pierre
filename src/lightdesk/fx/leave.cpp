/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
    Copyright (C) 2020  Tim Hughey

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

#include "lightdesk/fx/leave.hpp"
#include "lightdesk/faders/color/toblack.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

using namespace fader;

typedef color::ToColor<EasingInSine> FaderIn;
typedef color::ToColor<EasingOutSine> FaderOut;

Leave::Leave() {
  main = unit<PinSpot>("main");
  fill = unit<PinSpot>("fill");
}

void Leave::execute(audio::spPeaks peaks) {
  peaks.reset(); // no use for peaks

  auto bright = lightdesk::Color(0xff144a);
  auto dim = lightdesk::Color::black();

  static bool once = true;
  if (once) {
    unit<LedForest>("led forest")->leave();
    unit<ElWire>("el dance")->leave();
    unit<ElWire>("el entry")->leave();
    unit<DiscoBall>("discoball")->leave();

    main->activate<FaderIn>({.origin = dim, .dest = bright, .ms = 1000});
    fill->color(dim);
    once = false;
    return;
  }

  if (main->isFading() == false) {

    switch (next) {
    case 0:
      main->activate<FaderOut>({.origin = bright, .dest = dim, .ms = 1000});
      fill->activate<FaderIn>({.origin = dim, .dest = bright, .ms = 1000});
      break;

      // case 1:
      //   main->activate<FaderIn>({.origin = bright, .dest = bright, .ms =
      //   5000}); fill->activate<FaderIn>({.origin = bright, .dest = bright,
      //   .ms = 5000}); break;

    case 1:
      main->activate<FaderIn>({.origin = dim, .dest = bright, .ms = 1000});
      fill->activate<FaderOut>({.origin = bright, .dest = dim, .ms = 1000});

      break;

    default:
      main->activate<FaderOut>({.origin = bright, .dest = dim, .ms = 1000});
      fill->activate<FaderOut>({.origin = bright, .dest = dim, .ms = 1000});
      break;
    }

    next++;

    if (next >= 2) {
      next = 0;
    }
  }
}

} // namespace fx
} // namespace lightdesk
} // namespace pierre
