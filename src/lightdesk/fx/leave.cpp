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

namespace pierre {
namespace lightdesk {
namespace fx {

Leave::Leave() {
  _pinspots[0] = unit<PinSpot>("main");
  _pinspots[1] = unit<PinSpot>("fill");
}

void Leave::execute(audio::spPeaks peaks) {
  peaks.reset(); // no use for peaks

  if (_pinspots[0]->isFading() == false) {
    for (auto p : _pinspots) {
      Fader::Opts opts{
          .origin = Color(0xff144a), .dest = Color::black(), .ms = 1000};

      p->activate<Fader>(opts);
    }
  }

  static bool once = true;
  if (once) {
    unit<LedForest>("led forest")->leave();
    unit<ElWire>("el dance")->leave();
    unit<ElWire>("el entry")->leave();
    unit<DiscoBall>("discoball")->leave();

    once = false;
  }
}

} // namespace fx
} // namespace lightdesk
} // namespace pierre
