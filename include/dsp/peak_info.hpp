/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#pragma once

#include "base/elapsed.hpp"
#include "base/pe_time.hpp"

#include "dsp/peaks.hpp"

namespace pierre {

struct PeakInfo {
  uint32_t seq_num;
  uint32_t timestamp;
  shPeaks left;
  shPeaks right;
  bool silence;
  Nanos nettime_now;
  Nanos frame_localtime;
  Elapsed &uptime;
};

} // namespace pierre
