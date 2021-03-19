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

#ifndef pierre_lightdesk_headunits_elwire_hpp
#define pierre_lightdesk_headunits_elwire_hpp

#include "lightdesk/headunits/pwm.hpp"

namespace pierre {
namespace lightdesk {

class ElWire : public PulseWidthHeadUnit {

public:
  ElWire(uint8_t pwm_num) : PulseWidthHeadUnit(pwm_num) {
    config.dim = unitPercent(0.03);
    config.pulse_start = unitPercent(0.25);
    config.pulse_end = config.dim;

    snprintf(_id.data(), _id.size(), "EL%u", _address);
  }
};

typedef std::shared_ptr<ElWire> spElWire;

} // namespace lightdesk
} // namespace pierre

#endif
