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
#include "desk/fx.hpp"
#include "desk/fx/names.hpp"

#include <memory>

namespace pierre {
namespace fx {

class AllStop : public FX {
public:
  AllStop() noexcept : FX() {

    // prevent frames from being sent to the dmx controller
    should_render = false;

    // never finished unless a non-silent frame is sent for execute
    set_finished(false);
  }

  csv name() const override { return fx_name::ALL_STOP; }

public:
  static constexpr csv module_id{"fx.all_stop"};
};

} // namespace fx
} // namespace pierre
