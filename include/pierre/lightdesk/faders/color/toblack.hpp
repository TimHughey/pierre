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

#ifndef pierre_lightdesk_fader_to_black_hpp
#define pierre_lightdesk_fader_to_black_hpp

#include "lightdesk/color.hpp"
#include "lightdesk/faders/color/tocolor.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {
namespace fader {
namespace color {

// E = easing
template <typename E> class ToBlack : public ToColor<E> {

public:
  struct Opts {
    lightdesk::Color origin;
    ulong ms;
  };

public:
  ToBlack(const Opts &opts)
      : ToColor<E>({.origin = opts.origin,
                    .dest = lightdesk::Color::black(),
                    .ms = opts.ms}) {}
};
} // namespace color
} // namespace fader
} // namespace lightdesk
} // namespace pierre

#endif
