//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include "fader/fader.hpp"
#include "base/dura.hpp"

namespace pierre {
namespace desk {

bool Fader::travel() {
  if (fader_progress == 0.0) {
    // the first invocation (frame 0) represents the origin and start time of the fader
    start_at = dura::now_monotonic();
    fader_progress = 0.0001;

  } else {
    auto elapsed = dura::elapsed_abs(start_at);

    if (elapsed < duration) {
      fader_progress = doTravel(elapsed.count(), duration.count());
    } else {
      doFinish();
      finished = true;
    }
  }

  frames.count++;

  return !finished;
}

} // namespace desk
} // namespace pierre
