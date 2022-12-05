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

class AllStop : public FX, std::enable_shared_from_this<AllStop> {
public:
  AllStop() noexcept : FX() {
    set_finished(false);

    // note:  should render is set to true at creation so once() is
    //        executed and the data msg sent
    should_render = true;

    next_fx = fx_name::MAJOR_PEAK;
  }

  // do nothing, enjoy the silence
  void execute(Peaks &peaks) override {
    if (peaks.silence() == false) {
      next_fx = fx_name::MAJOR_PEAK;
      set_finished(true);
    }

    // note:  set should_render to false for subsequent executions to notify
    //        caller that we are in a shutdown state
    should_render = false;
  };

  csv name() const override { return fx_name::ALL_STOP; }

  void once() override { units(unit_name::AC_POWER)->dark(); }

  std::shared_ptr<FX> ptr_base() noexcept override {
    return std::static_pointer_cast<FX>(shared_from_this());
  }

public:
  static constexpr csv module_id{"ALL_STOP"};
};

} // namespace fx
} // namespace pierre
