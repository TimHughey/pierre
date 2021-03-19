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

#include "lightdesk/headunits/pinspot/pinspot.hpp"

namespace pierre {
namespace lightdesk {

PinSpot::PinSpot(uint16_t address) : HeadUnit(address, 6) {}

PinSpot::~PinSpot() {}

void PinSpot::autoRun(FxType_t fx) {
  _fx = fx;
  _mode = AUTORUN;
}

uint8_t PinSpot::autorunMap(FxType_t fx) const {
  static const uint8_t model_codes[] = {0,   31,  63,  79,  95,  111, 127, 143,
                                        159, 175, 191, 207, 223, 239, 249, 254};

  const uint8_t model_num = static_cast<uint8_t>(fx);

  uint8_t selected_model = 0;

  if (model_num < sizeof(model_codes)) {
    selected_model = model_codes[model_num];
  }

  return selected_model;
}

void PinSpot::color(const Color_t &color, float strobe) {
  _color = color;

  if ((strobe >= 0.0) && (strobe <= 1.0)) {
    _strobe = (uint8_t)(_strobe_max * strobe);
  }

  _mode = COLOR;
}

void PinSpot::dark() {
  _color = Color::black();
  _fx = fxNone;
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

void PinSpot::fadeTo(const Color_t &dest, float secs, float accel) {
  const FaderOpts fo{
      .origin = _color, .dest = dest, .travel_secs = secs, .accel = accel};
  fadeTo(fo);
}

void PinSpot::fadeTo(const FaderOpts_t &fo) {
  const Color_t &origin = faderSelectOrigin(fo);

  _fader.prepare(origin, fo);

  _mode = FADER;
}

void PinSpot::frameUpdate(dmx::UpdateInfo &info) {
  dmx::Frame snippet;
  snippet.assign(_frame_len, 0x00);

  // always copy the color, only not used when mode is AUTORUN
  // _color.copyToByteArray(&(data[1])); // color data bytes 1-5

  switch (_mode) {
  case DARK:
    // no changes required to initialized snippet
    break;

  case COLOR:
  case FADER:
    _color.copyTo(snippet, 1);

    if (_strobe > 0) {
      snippet[0] = _strobe + 0x87;
    } else {
      snippet[0] = 0xF0;
    }

    snippet[5] = 0x00; // clear autorun
    break;

  case AUTORUN:
    snippet[5] = autorunMap(_fx);
    break;
  }

  auto frame_byte = info.frame.begin() + _address;

  for (auto byte : snippet) {
    *frame_byte++ = byte;
  }
}

} // namespace lightdesk
} // namespace pierre
