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

namespace pierre {

void FX::executeLoop(shPeaks peaks) {
  if (called_once == false) {
    // frame 0 is always consumed by the call to once()
    once();
    called_once = true;
  } else {
    // frame n is passed to executeFX
    execute(peaks);
  }
}

// void FX::execute(shPeaks peaks) {
//   State::silent(peaks->silence());

//   // onceWrapper returns true if it called once() and consumes the first
//   // frame of the FX
//   if (callOnce() == true) {
//     return;
//   }

//   // the second frame is the first call to executeFX()
//   executeFX(move(peaks));
// }

} // namespace pierre
