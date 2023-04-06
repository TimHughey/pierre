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
#include "desk/msg/data.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/names.hpp"
#include "frame/peaks.hpp"

namespace pierre {
namespace desk {

Units FX::units; // headunits available for FX (static class data)

void FX::ensure_units() noexcept {
  if (units.empty()) units.create_all_from_cfg(); // create the units once
}

void FX::execute(Peaks peaks) noexcept { peaks.noop(); };

bool FX::render(Peaks &&peaks, DataMsg &msg) noexcept {
  if (should_render) {
    if (called_once == false) {
      once();                // frame 0 consumed by call to once(), peaks not rendered
      units.update_msg(msg); // ensure any once() actions are in the data msg

      called_once = true;

    } else {
      units.prepare();           // do any prep required to render next frame
      execute(std::move(peaks)); // render frame into data msg
      units.update_msg(msg);     // populate data msg
    }
  }

  return !finished;
}

} // namespace desk
} // namespace pierre
