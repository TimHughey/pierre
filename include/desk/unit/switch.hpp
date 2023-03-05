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

#include "desk/unit.hpp"

namespace pierre {
namespace desk {

class Switch : public Unit {
public:
  Switch(const auto &opts) noexcept : Unit(opts), powered{true} {}

public:
  // required by FX
  void activate() noexcept override { on(); }
  void dark() noexcept override { off(); }

  // specific to this unit
  void on() noexcept { powered = true; }
  void off() noexcept { powered = false; }

  void update_msg(DataMsg &msg) noexcept override { msg.add_kv(name, powered); }

private:
  bool powered;
};

} // namespace desk
} // namespace pierre
