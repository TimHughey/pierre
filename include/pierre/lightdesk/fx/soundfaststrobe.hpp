/*
    lightdesk/fx/soundfaststrobe.hpp -- PinSpot Auto Sound with flourishes
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

#ifndef pierre_lightdesk_fx_soundfaststrobe_hpp
#define pierre_lightdesk_fx_soundfaststrobe_hpp

#include "lightdesk/fx/complexity.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class SoundFastStrobe : public Complexity {
public:
  SoundFastStrobe() : Complexity(fxFastStrobeSound) { _roll = roll2D6(); }

  void executeEffect() {
    if (onetime()) {
      pinSpotMain()->autoRun(fxFastStrobeSound);

      FaderOpts fader{
          .dest = Color::black(), .travel_secs = 0.0f, .use_origin = true};

      switch (_roll) {
      case 2:
        pinSpotFill()->color(Color::red());
        break;

      case 3:
        pinSpotFill()->color(Color(0, 0, 0, 32));
        break;

      case 4:
        pinSpotFill()->color(Color(0, 0, 32, 0), 0.50f);

        break;

      case 5:
        pinSpotFill()->color(Color(0, 32, 0, 0), 0.75f);
        break;

      case 6:
        pinSpotFill()->dark();
        break;

      case 7:
        fader.dest = Color::randomize();
        fader.travel_secs = runtimePercent(_runtimes[_roll]);
        pinSpotFill()->fadeTo(fader);
        break;

      case 8:
        pinSpotFill()->autoRun(fxFastStrobeSound);
        break;

      case 9:
        pinSpotFill()->autoRun(fxRedBlueGradient);
        break;

      case 10:
        fader.dest = Color::randomize();
        fader.travel_secs = runtimePercent(_runtimes[_roll]);
        pinSpotFill()->fadeTo(fader);
        break;

      case 11:
        pinSpotFill()->color(Color::blue());
        break;

      case 12:
        pinSpotFill()->color(Color::green());
        break;
      }
    }
  }

private:
  uint8_t _roll = 0;
  const float _runtimes[13] = {0.0f,  0.0f,  0.25f, 0.20f, 0.15f, 0.15f, 0.13f,
                               0.13f, 0.13f, 0.13f, 0.17f, 0.18f, 0.18f};
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
