// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "desk/fx.hpp"
#include "base/logger.hpp"
#include "desk/data_msg.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/names.hpp"
#include "frame/frame.hpp"

#include <algorithm>
#include <ranges>

namespace pierre {

Units FX::units; // headunits available for FX (static class data)

FX::FX() noexcept : finished(false) {
  if (units.empty()) { // create the units once
    units.add<AcPower>({.name = unit_name::AC_POWER, .address = 0});
    units.add<PinSpot>({.name = unit_name::MAIN_SPOT, .address = 1});
    units.add<PinSpot>({.name = unit_name::FILL_SPOT, .address = 7});
    units.add<DiscoBall>({.name = unit_name::DISCO_BALL, .address = 3});
    units.add<ElWire>({.name = unit_name::EL_DANCE, .address = 2});
    units.add<ElWire>({.name = unit_name::EL_ENTRY, .address = 4});
    units.add<LedForest>({.name = unit_name::LED_FOREST, .address = 1});
  }
}

bool FX::match_name(const std::initializer_list<csv> names) const noexcept {
  return std::ranges::any_of(names, [this](const auto &n) { return n == name(); });
}

bool FX::render(frame_t frame, desk::DataMsg &msg) noexcept {

  if (called_once == false) {
    once();                // frame 0 consumed by call to once(), peaks not rendered
    units.update_msg(msg); // ensure any once() actions are in the data msg

    called_once = true;

  } else {
    units.prepare();       // do any prep required to render next frame
    execute(frame->peaks); // render frame into data msg
    units.update_msg(msg); // populate data msg
  }

  return finished.load();
}

} // namespace pierre
