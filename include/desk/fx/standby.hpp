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

#include "base/conf/token.hpp"
#include "base/types.hpp"
#include "desk/color.hpp"
#include "desk/fx.hpp"
#include "frame/peaks.hpp"

#include <memory>

namespace pierre {
namespace desk {

class Standby : public FX {

public:
  Standby(auto &executor) noexcept : FX(executor, fx::STANDBY), tokc(module_id) { apply_config(); }

  void execute(const Peaks &peaks) noexcept override;

  void once() noexcept override final;

private:
  void apply_config() noexcept;

private:
  // order dependent
  conf::token tokc;

  // order independent
  Color first_color;
  Color next_color;
  double hue_step{0.0};
  double max_brightness{0};
  double next_brightness{0};

public:
  static constexpr csv module_id{"fx.standby"};
};

} // namespace desk
} // namespace pierre
