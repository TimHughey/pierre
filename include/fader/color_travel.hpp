/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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
#include "base/types.hpp"
#include "fader/easings.hpp"
#include "fader/fader.hpp"

namespace pierre {
namespace fader {

struct Opts {
  Color origin;
  Color dest;
  Nanos duration{0};
};

class ColorTravel : public Fader {
public:
  ColorTravel(const Opts &opts) : Fader(opts.duration), origin(opts.origin), dest(opts.dest) {}

  virtual void doFinish() override { pos = dest; }
  virtual float doTravel(const float current, const float total) = 0;
  const Color &position() const override { return pos; }

protected:
  const Color origin;
  const Color dest;

  Color pos; // current fader position
};

} // namespace fader
} // namespace pierre
