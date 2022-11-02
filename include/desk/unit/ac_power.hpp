/*
    lightdesk/headunits/discoball.hpp - Ruth LightDesk Headunit Disco Ball
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

#include "desk/unit.hpp"
#include <memory>

namespace pierre {

class AcPower : public Unit {
public:
  AcPower(const unit::Opts opts) noexcept : Unit(opts), powered{false} {}

public: // effects
  void dark() override { off(); }
  void leave() override { on(); }
  void prepare() override {}
  void on() noexcept { powered = true; }
  void off() noexcept { powered = false; }

  void update_msg(desk::DataMsg &msg) override { msg.doc[unitName()] = powered; }

private:
  bool powered;
};

} // namespace pierre