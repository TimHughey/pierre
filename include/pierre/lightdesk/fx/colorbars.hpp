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

#ifndef pierre_lightdesk_fx_colorbars_hpp
#define pierre_lightdesk_fx_colorbars_hpp

#include "lightdesk/fx/base.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class ColorBars : public FxBase {
public:
  ColorBars() : FxBase(fxColorBars) {
    countMax() = 10;
    count() = 10;
  }

protected:
  void executeEffect() {
    PinSpot_t *pinspot = nullptr;

    if (pinSpotMain()->isFading() || pinSpotFill()->isFading()) {
      // this is a no op while the PinSpots are fading
      return;
    }

    // ok, we're not fading so we must do something
    const auto pinspot_select = count() % 2;
    if (pinspot_select == 0) {
      // evens we act on the main pinspot
      pinspot = pinSpotMain();
    } else {
      pinspot = pinSpotFill();
    }

    switch (count()) {
    case 1:
      completed();
      break;

    case 2:
      pinSpotMain()->color(Color::black());
      pinSpotFill()->color(Color::black());
      break;

    case 3:
    case 4:
      _fade.origin = Color::bright();
      break;

    case 5:
    case 6:
      _fade.origin = Color::blue();
      break;

    case 7:
    case 8:
      _fade.origin = Color::green();
      break;

    case 9:
    case 10:
      _fade.origin = Color::red();
      break;
    }

    if (count() > 2) {
      pinspot->fadeTo(_fade);
    }

    count()--;
  }

private:
  FaderOpts_t _fade{.origin = Color::black(),
                    .dest = Color::black(),
                    .travel_secs = 0.3f,
                    .use_origin = true};
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
