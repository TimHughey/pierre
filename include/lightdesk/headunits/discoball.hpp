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

#include "lightdesk/headunits/pwm.hpp"

namespace pierre {
namespace lightdesk {

class DiscoBall : public PulseWidthHeadUnit {

public:
  DiscoBall(uint8_t pwm_num) : PulseWidthHeadUnit(pwm_num) {
    config.leave = 0;

    _id[0] = 'D';
    _id[1] = 'S';
    _id[2] = 'B';
  }

public: // effects
  inline void spin() { percent(0.65); }
  inline void still() { dark(); }
};

typedef std::shared_ptr<DiscoBall> spDiscoBall;

} // namespace lightdesk
} // namespace pierre