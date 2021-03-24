/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
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

#include "audio/peaks.hpp"

namespace pierre {
namespace audio {

Peak::Config Peak::_cfg{.mag{.floor = 36500.0, .strong = 3.0}};

bool Peaks::bass() const {
  bool bass = false;

  for (const Peak &peak : _peaks) {
    if (!peak.magStrong()) {
      break;
    } else if ((peak.freq > 30.0) && (peak.freq <= 170.0)) {
      bass = true;
      break;
    }
  }

  return bass;
}

bool Peaks::hasPeak(PeakN n) const {
  bool rc = false;

  // PeakN_t reprexents the peak of interest in the range of 1..max_peaks
  if (_peaks.size() && (_peaks.size() > n)) {
    rc = true;
  }

  return rc;
}

const Peak Peaks::majorPeak() const { return std::move(peakN(1)); }

const Peak Peaks::peakN(const PeakN n) const {
  Peak peak;

  if (hasPeak(n)) {
    const Peak check = _peaks.at(n - 1);

    if (check.mag > Peak::magFloor()) {
      peak = check;
    }
  }

  return std::move(peak);
}

void Peaks::sort() {
  std::sort(_peaks.begin(), _peaks.end(),
            [](const Peak &lhs, const Peak &rhs) { return lhs.mag > rhs.mag; });
}

} // namespace audio
} // namespace pierre
