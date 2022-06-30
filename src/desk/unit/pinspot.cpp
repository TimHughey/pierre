/* devs/pinspot]/base.cpp - Ruth Pinspot Device Copyright (C) 2020  Tim Hughey

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "desk/unit/pinspot.hpp"
#include "base/typical.hpp"

namespace pierre {

void PinSpot::colorNow(const Color &color_now, float strobe_val) {
  color = color_now;

  if ((strobe_val >= 0.0) && (strobe_val <= 1.0)) {
    strobe = (uint8_t)(strobe_max * strobe);
  }

  fx = FX::None;
}

void PinSpot::faderMove() {
  if (isFading()) {
    auto continue_traveling = fader->travel();
    color = fader->position();
    strobe = 0;

    if (continue_traveling == false) {
      fader.reset();
    }
  }
}

void PinSpot::frameUpdate(packet::DMX &packet) {
  auto snippet = packet.frameData() + address;

  color.copyRgbToByteArray(snippet + 1);

  if (strobe > 0) {
    snippet[0] = strobe + 0x87;
  } else {
    snippet[0] = 0xF0;
  }

  snippet[5] = fx;
}

} // namespace pierre
