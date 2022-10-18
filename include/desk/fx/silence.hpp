/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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
#include "desk/fx.hpp"
#include "desk/fx/names.hpp"
#include "frame/peaks.hpp"

#include <memory>

namespace pierre {
namespace fx {

class Silence : public FX {
public:
  Silence() = default;

  void execute(peaks_t peaks) override {
    peaks.reset();
    // do nothing, enjoy the silence
  };

  csv name() const override { return fx::SILENCE; }
};

} // namespace fx
} // namespace pierre
