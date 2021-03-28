/*
    lightdesk/fx/majorpeak.hpp -- LightDesk Effect Major Peak
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

#ifndef pierre_lightdesk_fx_leave_hpp
#define pierre_lightdesk_fx_leave_hpp

#include <array>

#include "lightdesk/fx/fx.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class Leave : public Fx {

public:
  Leave();
  ~Leave() = default;

  void execute(audio::spPeaks peaks) override;
  const string_t &name() const override {
    static const string_t fx_name = "Leave";

    return fx_name;
  }

private:
  spPinSpot main;
  spPinSpot fill;

  int next = 0;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
