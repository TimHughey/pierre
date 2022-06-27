/*
    lightdesk/headunits/elwire.hpp - Ruth LightDesk Headunit EL Wire
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

class ElWire : public PulseWidthHeadUnit {

public:
  ElWire(uint8_t pwm_num) : PulseWidthHeadUnit(pwm_num) {
    config.max = unitPercent(0.25);
    config.min = unitPercent(0.01);
    config.dim = unitPercent(0.03);
    config.pulse_start = unitPercent(0.15);
    config.pulse_end = config.dim;
    config.leave = unitPercent(0.50);

    snprintf(_id.data(), _id.size(), "EL%u", _address);

    dim();
  }
};

typedef std::shared_ptr<ElWire> spElWire;

} // namespace lightdesk
} // namespace pierre
