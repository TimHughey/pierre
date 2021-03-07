/*
    lightdesk/control.hpp - Ruth Light Desk Control
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

#ifndef pierre_lightdesk_control_hpp
#define pierre_lightdesk_control_hpp

#include "lightdesk/lightdesk.hpp"
#include "local/types.hpp"

namespace pierre {

typedef class LightDeskControl LightDeskControl_t;

class LightDeskControl {
public:
  LightDeskControl() { LightDesk::preStart(); };

  bool config();

  I2s_t *i2s() const { return _desk->i2s(); }
  bool isRunning() const { return (_desk == nullptr) ? false : true; }
  bool handleCommand(MsgPayload_t &msg);

  static bool objectSizes();

  bool reportStats() {
    auto rc = true;

    if (_desk == nullptr) {
      printf("LightDesk offline\n");
      rc = false;
    } else {
      rc = stats();
    }

    return rc;
  }

  // available functionality
public:
  inline bool color(PinSpotFunction_t func, Rgbw_t rgbw, float strobe = 0.0f) {
    _request = Request(COLOR, func, rgbw, strobe);
    return setMode();
  }

  inline bool dance(const float secs) {
    _request = Request(DANCE, secs);
    return setMode();
  }

  inline bool dark() {
    _request = Request(DARK);
    return setMode();
  }

  inline bool fadeTo(PinSpotFunction_t func, Rgbw_t rgbw, float secs = 1.0) {
    _request = Request(FADE_TO, func, rgbw, secs);
    return setMode();
  }

  inline bool majorPeak() {
    _request = Request(MAJOR_PEAK);
    return setMode();
  }

  inline bool ready() {
    _request = Request(READY);
    return setMode();
  }

  inline bool stop() {
    auto rc = true;

    if (isRunning()) {
      _request = Request(STOP);
      rc = setMode();

      if (_desk != nullptr) {
        delete _desk;
        _desk = nullptr;
      }
    }

    return rc;
  }

private:
  bool setModeDance(const float secs);
  bool setMode() { return setMode(_request.mode()); }
  bool setMode(LightDeskMode_t mode);
  bool stats();

private:
  LightDeskMode_t _mode = INIT;
  LightDesk *_desk = nullptr;
  Request_t _request;
};

} // namespace pierre

#endif
