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
#include "base/typical.hpp"
#include "fader/to_color.hpp"

namespace pierre {
namespace fader {

// E = easing
template <typename E> class ToBlack : public ToColor<E> {
public:
  struct Opts {
    Color origin;
    Nanos duration;
  };

public:
  ToBlack(const Opts &opts)
      : ToColor<E>({.origin = opts.origin, .dest = Color::black(), .duration = opts.duration}) {}
};
} // namespace fader
} // namespace pierre