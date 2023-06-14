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
#include "desk/color/hsb.hpp"
#include "desk/fader/easings.hpp"
#include "desk/fader/fader.hpp"

namespace pierre {
namespace desk {
namespace fader {

template <typename Easing> class ColorTravel : public Fader {
public:
  ColorTravel(const Hsb &origin, const Nanos &duration) noexcept
      : Fader(duration), origin(origin), dest(Hsb{}) {}

  virtual void doFinish() override { pos = dest; }
  virtual double doTravel(double current, double total) noexcept override {
    Bri fade_level(easing.calc(current, total));

    if (origin.black()) {
      pos = dest;
      pos.bri = dest.bri * fade_level;

    } else if (dest.black()) {
      pos = origin;
      pos.bri -= origin.bri * fade_level;

    } else {
      // pos = Hsb::interpolate(origin, dest, fade_level);
    }

    return (double)fade_level;
  }

  Hsb position() const noexcept override { return pos; }

protected:
  Hsb origin;
  Hsb dest;

  Hsb pos; // current fader position

  Easing easing;
};

} // namespace fader
} // namespace desk
} // namespace pierre
