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

#pragma once

#include "base/types.hpp"
#include "desk/color.hpp"
#include "fader/color_travel.hpp"
#include "fader/easings.hpp"
#include "fader/fader.hpp"

namespace pierre {
namespace fader {

template <typename E> class ToColor : public ColorTravel {
public:
  ToColor(const Opts &opts) : ColorTravel(opts) {
    if (origin.isBlack()) {
      pos = dest;
      pos.setBrightness(origin);
    } else {
      pos = origin;
    }
  }

protected:
  virtual float doTravel(const float current, const float total) override {
    auto fade_level = easing.calc(current, total);

    if (origin.isBlack()) {
      auto brightness = dest.brightness();
      pos.setBrightness(brightness * fade_level);

    } else if (dest.isBlack()) {
      auto brightness = origin.brightness();
      pos.setBrightness(brightness - (brightness * fade_level));

    } else {
      pos = Color::interpolate(origin, dest, fade_level);
    }

    return fade_level;
  }

private:
  E easing;
};

} // namespace fader
} // namespace pierre
