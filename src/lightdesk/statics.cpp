/*
    lightdesk/statics.cpp - Ruth LightDesk Statics Definition
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

#include "lightdesk/fx/majorpeak.hpp"
#include "lightdesk/headunits/pinspot/base.hpp"
#include "lightdesk/headunits/pinspot/color.hpp"
#include "lightdesk/headunits/pwm.hpp"

namespace pierre {
namespace lightdesk {

float Color::_scale_min = 0;
float Color::_scale_max = 0;

namespace fx {
FxConfig_t FxBase::_cfg;
FxStats_t FxBase::_stats;

MajorPeak::FreqColorList_t MajorPeak::_freq_colors = {};

} // namespace fx
} // namespace lightdesk
} // namespace pierre
