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

#include "lightdesk/faders/color/toblack.hpp"
#include "lightdesk/fx/fx.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

using namespace lightdesk::fader;
typedef color::ToColor<EasingToZeroSine> Fader;

class ColorBars : public Fx {
public:
  ColorBars() : Fx() {
    main = unit<PinSpot>("main");
    fill = unit<PinSpot>("fill");
  }

  void execute(audio::spPeaks peaks) override {
    peaks.reset(); // this fx does not use peaks

    PinSpot *pinspot = nullptr;

    Fader::Opts opts{.origin = lightdesk::Color(),
                     .dest = lightdesk::Color::black(),
                     .ms = 300};

    if (main->isFading() || fill->isFading()) {
      // this is a no op while the PinSpots are fading
      return;
    }

    // ok, we're not fading so we must do something
    const auto pinspot_select = count() % 2;
    if (pinspot_select == 0) {
      // evens we act on the main pinspot
      pinspot = main.get();
    } else {
      pinspot = fill.get();
    }

    switch (count()) {
    case 1:
      _finished = true;
      break;

    case 2:
      main->color(lightdesk::Color::black());
      fill->color(lightdesk::Color::black());
      break;

    case 3:
    case 4:
      opts.origin = lightdesk::Color::full();
      break;

    case 5:
    case 6:
      opts.origin = lightdesk::Color(0x0000ff);
      break;

    case 7:
    case 8:
      opts.origin = lightdesk::Color(0x00ff00);
      break;

    case 9:
    case 10:
      opts.origin = lightdesk::Color(0xff0000);
      break;
    }

    if (count() > 2) {
      pinspot->activate<Fader>(opts);
    }

    count()--;
  }

  const string_t &name() const override {
    static const string_t fx_name = "ColorBars";

    return fx_name;
  }

private:
  uint &count() {
    static uint x = 10;

    return x;
  }

private:
  spPinSpot main;
  spPinSpot fill;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
