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

    https://www.wisslanding.com */

#include "lightdesk/headunits/pinspot.hpp"

namespace pierre {
namespace lightdesk {

PinSpot::PinSpot(uint16_t address) : HeadUnit(address, 6) {}

PinSpot::~PinSpot() {}

void PinSpot::autoRun(Fx fx) {
  _fx = fx;
  _mode = AUTORUN;
}

void PinSpot::color(const Color &color, float strobe) {
  _color = color;

  if ((strobe >= 0.0) && (strobe <= 1.0)) {
    _strobe = (uint8_t)(_strobe_max * strobe);
  }

  _mode = COLOR;
}

void PinSpot::dark() {
  _color = Color::black();
  _fx = Fx::None;
  _mode = DARK;
}

void PinSpot::faderMove() {
  auto continue_traveling = _fader.travel();
  _color = _fader.location();
  _strobe = 0;

  if (continue_traveling == false) {
    _mode = COLOR;
  }
}

void PinSpot::fadeTo(const Color &dest, float secs, float accel) {
  const FaderOpts fo{
      .origin = _color, .dest = dest, .travel_secs = secs, .accel = accel};
  fadeTo(fo);
}

void PinSpot::fadeTo(const FaderOpts_t &fo) {
  const Color &origin = faderSelectOrigin(fo);

  _fader.prepare(origin, fo);

  _mode = FADER;
}

void PinSpot::frameUpdate(dmx::Packet &packet) {
  auto snippet = packet.frameData() + _address;

  _color.copyRgbToByteArray(snippet + 1);

  if (_strobe > 0) {
    snippet[0] = _strobe + 0x87;
  } else {
    snippet[0] = 0xF0;
  }

  snippet[5] = _fx;
}

} // namespace lightdesk
} // namespace pierre
