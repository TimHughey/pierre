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

#include <esp_log.h>

#include "lightdesk/headunits/pinspot/base.hpp"

namespace pierre {
namespace lightdesk {

PinSpot::PinSpot(uint16_t address) : HeadUnit(address, 6) {}

PinSpot::~PinSpot() {}

void  PinSpot::autoRun(FxType_t fx) {
  _fx = fx;
  _mode = AUTORUN;
  frameUpdate();
}

uint8_t  PinSpot::autorunMap(FxType_t fx) const {
  static const uint8_t model_codes[] = {0,   31,  63,  79,  95,  111, 127, 143,
                                        159, 175, 191, 207, 223, 239, 249, 254};

  const uint8_t model_num = static_cast<uint8_t>(fx);

  uint8_t selected_model = 0;

  if (model_num < sizeof(model_codes)) {
    selected_model = model_codes[model_num];
  }

  return selected_model;
}

void  PinSpot::color(const Color_t &color, float strobe) {
  _color = color;

  if ((strobe >= 0.0) && (strobe <= 1.0)) {
    _strobe = (uint8_t)(_strobe_max * strobe);
  }

  _mode = COLOR;

  frameUpdate();
}

void  PinSpot::dark() {
  _color = Color::black();
  _fx = fxNone;
  _mode = DARK;

  frameUpdate();
}

void  PinSpot::faderMove() {
  auto continue_traveling = _fader.travel();
  _color = _fader.location();
  _strobe = 0;

  frameUpdate();

  if (continue_traveling == false) {
    _mode = HOLD;
  }
}

void  PinSpot::fadeTo(const Color_t &dest, float secs, float accel) {
  const FaderOpts fo{
      .origin = _color, .dest = dest, .travel_secs = secs, .accel = accel};
  fadeTo(fo);
}

void  PinSpot::fadeTo(const FaderOpts_t &fo) {
  const Color_t &origin = faderSelectOrigin(fo);

  _fader.prepare(origin, fo);

  _mode = FADER;
}

void  PinSpot::frameUpdate() {

  uint8_t *data = frameData();   // HeadUnit member function
  auto need_frame_update = true; // true is most common

  data[0] = 0x00; // head control byte

  // always copy the color, only not used when mode is AUTORUN
  _color.copyToByteArray(&(data[1])); // color data bytes 1-5

  switch (_mode) {
  case HOLD:
    need_frame_update = false;
    break;

  case DARK:
    data[5] = 0x00; // clear autorun
    _mode = HOLD;
    break;

  case COLOR:
  case FADER:
    if (_strobe > 0) {
      data[0] = _strobe + 0x87;
    } else {
      data[0] = 0xF0;
    }

    data[5] = 0x00; // clear autorun
    break;

  case AUTORUN:
    data[5] = autorunMap(_fx);
    break;
  }

  // the changes to the frame for this PinSpot were staged above. calling
  // frameChanged() will alert Dmx to incorporate the staged changes into the
  // next frame
  frameChanged() = need_frame_update;
}

} // namespace lightdesk
} // namespace pierre
