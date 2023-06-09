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
#include "desk/fx.hpp"
#include "desk/fx/names.hpp"

#include <memory>

namespace pierre {

namespace desk {

/// @brief AllStop FX signals to stop all rendering
class AllStop : public FX {
public:
  /// @brief Create object with no next FX and never render
  AllStop() noexcept : FX(fx::ALL_STOP, fx::NONE, FX::NoRender) {}

public:
  MOD_ID("fx.all_stop");
};

} // namespace desk
} // namespace pierre
