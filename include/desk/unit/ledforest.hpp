/*
    lightdesk/headunits/ledforest.hpp - Ruth LightDesk Headunit LED Forest
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

#include "desk/unit/pwm.hpp"

#include <memory>

namespace pierre {

class LedForest;
typedef std::shared_ptr<LedForest> shLedForest;

class LedForest : public PulseWidth {
public:
  LedForest(const unit::Opts opts) : PulseWidth(opts) {
    config.dim = unitPercent(0.005);
    config.pulse_start = unitPercent(0.02);
    config.pulse_end = config.dim;

    _id[0] = 'L';
    _id[1] = 'F';
    _id[2] = 'R';

    dim();
  }
};

} // namespace pierre
