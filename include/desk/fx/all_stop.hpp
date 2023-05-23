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

namespace desk {

/// @brief AllStop FX with the sole objection of stopping all rendering
class AllStop : public FX {
public:
  /// @brief Create object
  /// @param fx_timer Timer to pass through to FX
  AllStop(auto &&fx_timer) noexcept
      : FX(std::forward<decltype(fx_timer)>(fx_timer), fx::ALL_STOP, fx::NONE, FX::NoRender) {

    // never finished unless a non-silent frame is sent for execute
    set_finished(false);
  }

public:
  MOD_ID("fx.all_stop");
};

} // namespace desk
} // namespace pierre
