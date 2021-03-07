/*
    lightdesk/request.hpp - Ruth Light Desk Request
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

#ifndef pierre_lightdesk_request_hpp
#define pierre_lightdesk_request_hpp

#include "lightdesk/types.hpp"

namespace pierre {
namespace lightdesk {

typedef class Request Request_t;

class Request {
public:
  Request(){};

  Request(LightDeskMode_t m) : _mode(m) {}

  Request(LightDeskMode_t m, float val) : _mode(m) {

    switch (m) {
    case DANCE:
      _dance.secs = val;
      break;

    case MAJOR_PEAK:
      break;

    default:
      break;
    }
  }

  Request(LightDeskMode_t m, const PinSpotFunction_t func, const Rgbw_t rgbw,
          const float f)
      : _mode(m), _func(func) {
    switch (m) {
    case FADE_TO:
      _fadeto.rgbw = rgbw;
      _fadeto.secs = f;
      break;

    case COLOR:
      _color.rgbw = rgbw;
      _color.strobe = f;
      break;

    default:
      _color.rgbw = 0;
      _color.strobe = 0.0f;
    }
  }

  Rgbw_t colorRgbw() const { return _color.rgbw; }
  Strobe_t colorStrobe() const { return _color.strobe; }
  float danceInterval() const { return _dance.secs; }
  Rgbw_t fadeToRgbw() const { return _fadeto.rgbw; }
  float fadeToSecs() const { return _fadeto.secs; }

  PinSpotFunction_t func() const { return _func; }

  LightDeskMode_t mode() const { return _mode; }

public:
  LightDeskMode_t _mode = INIT;
  PinSpotFunction_t _func = PINSPOT_NONE;

  union {
    struct {
      float secs;
    } _dance;

    struct {
      uint32_t rgbw;
      float strobe;
    } _color;

    struct {
      uint32_t rgbw;
      float secs;
    } _fadeto;
  };
};

} // namespace lightdesk
} // namespace pierre

#endif
