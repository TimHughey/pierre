/*
    devs/pinspot/fader.hpp - Ruth Pin Spot Fader Action
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

#ifndef pierre_lightdesk_fader_color_to_color_hpp
#define pierre_lightdesk_fader_color_to_color_hpp

#include "lightdesk/color.hpp"
#include "lightdesk/faders/color.hpp"
#include "lightdesk/faders/easings.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {
namespace fader {
namespace color {

template <typename E> class ToColor : public Color {

public:
  ToColor(const Color::Opts &opts) : Color(opts) { _location = _origin; }

protected:
  virtual void handleTravel(const float progress) {
    E e;
    auto fade_level = e.calc(progress);

    if (_origin.isBlack() || (_dest.isBlack())) {

      if (e.out) {
        auto brightness = _origin.brightness();
        _location.setBrightness(brightness * fade_level);
      } else if (e.in) {
        auto brightness = _dest.brightness();
        _location = _dest;
        _location.setBrightness(brightness - (brightness * fade_level));
      }

    } else {
      _location = lightdesk::Color::interpolate(_origin, _dest, fade_level);
    }
  }
};

} // namespace color
} // namespace fader
} // namespace lightdesk
} // namespace pierre

#endif