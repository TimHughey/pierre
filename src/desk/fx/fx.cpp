/*
    Pierre - Custom Light Show for Wiss Landing
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

#include "desk/fx.hpp"
#include "desk/data_msg.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/opts.hpp"
#include "frame/frame.hpp"

namespace pierre {

shUnits FX::units; // static class member

FX::FX() {
  if (!units) { // create the units once
    units = std::make_shared<Units>();
    units->add<PinSpot>(unit::MAIN_SPOT_OPTS);
    units->add<PinSpot>(unit::FILL_SPOT_OPTS);
    units->add<DiscoBall>(unit::DISCO_BALL_OPTS);
    units->add<ElWire>(unit::EL_DANCE_OPTS);
    units->add<ElWire>(unit::EL_ENTRY_OPTS);
    units->add<LedForest>(unit::LED_FOREST_OPTS);
  }
}

void FX::render(frame_t frame, desk::DataMsg &msg) {
  if (called_once == false) {
    // frame 0 is always consumed by the call to once()
    once();
    called_once = true;
  } else {
    units->prepare();

    // frame n is passed to execute
    auto [left, right] = frame->peaks;
    execute(left);

    units->update_msg(msg);
  }
}

} // namespace pierre
