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

#pragma once

#include "base/color.hpp"
#include "fader/color_travel.hpp"
#include "fader/easings.hpp"

namespace pierre {
namespace fader {
namespace travel {

template <typename E> class ToColor : public fader::ColorTravel {
public:
  ToColor(const ColorTravel::ColorTravelOpts &opts) : ColorTravel(opts) {
    if (_origin.isBlack()) {
      _location = _dest;
      _location.setBrightness(_origin);
    } else {
      _location = _origin;
    }
  }

protected:
  virtual float handleTravel(const float current, const float total) override {
    auto fade_level = _easing.calc(current, total);

    if (_origin.isBlack()) {
      auto brightness = _dest.brightness();
      _location.setBrightness(brightness * fade_level);
    } else if (_dest.isBlack()) {
      auto brightness = _origin.brightness();
      _location.setBrightness(brightness - (brightness * fade_level));

    } else {
      _location = lightdesk::Color::interpolate(_origin, _dest, fade_level);
    }

    return fade_level;
  }

private:
  E _easing;
};

} // namespace travel
} // namespace fader
} // namespace pierre
