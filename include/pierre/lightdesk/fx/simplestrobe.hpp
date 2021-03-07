/*
    lightdesk/fx/simplestrobe.hpp -- PinSpots Strobing White and a color
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

#ifndef pierre_lightdesk_fx_simplestrobe_hpp
#define pierre_lightdesk_fx_simplestrobe_hpp

#include "lightdesk/fx/complexity.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class SimpleStrobe : public Complexity {
public:
  SimpleStrobe() : Complexity(fxSimpleStrobe) { runtimeReduceTo(0.37); }

  void executeEffect() {
    if (onetime()) {
      pinSpotMain()->color(Color::bright(), 0.55);

      switch (roll1D6()) {
      case 1:
      case 2:
        pinSpotFill()->color(Color::red(), 0.70);
        break;

      case 3:
      case 4:
        pinSpotFill()->color(Color::green(), 0.70);
        break;

      case 5:
      case 6:
        pinSpotFill()->color(Color::blue(), 0.70);
        break;
      }
    }
  }

private:
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
