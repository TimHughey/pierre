/*
    lightdesk/fx/basic.hpp -- LightDesk Effect Basic
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

#ifndef pierre_lightdesk_fx_basic_hpp
#define pierre_lightdesk_fx_basic_hpp

#include "lightdesk/fx/base.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class Basic : public FxBase {
public:
  Basic(const FxType type) : FxBase(type) {
    runtimeReduceTo(0.27f); // limit how long this effect runs
  }

  void executeEffect() {
    if (onetime()) {
      pinSpotMain()->autoRun(fx()); // always run the choosen fx on main

      const auto roll = roll1D6();

      // make basic Fx more interesting
      switch (roll) {
      case 1:
        pinSpotFill()->autoRun(fx());
        break;

      case 2:
        pinSpotFill()->color(Color::red(), 0.75f);
        break;

      case 3:
        pinSpotFill()->color(Color::blue(), 0.75f);
        break;

      case 4:
        pinSpotFill()->color(Color::green(), 0.75f);
        break;

      case 5:
        pinSpotFill()->autoRun(fxFastStrobeSound);
        break;

      case 6:
        pinSpotFill()->autoRun(fxColorCycleSound);
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
