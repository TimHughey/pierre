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

#include <cmath>

#include "audio/peaks.hpp"
#include "misc/elapsed.hpp"

using namespace std;

namespace pierre {
namespace audio {

namespace peak {
Scaled scaleVal(Unscaled val) {

  // log10 of 0 is undefined
  if (val <= 0.0f) {
    return 0.0f;
  }

  return 10.0f * log10(val);
}

Config Config::defaults() {
  auto cfg = Config();

  auto &mag = cfg.mag.minmax;
  auto &scale = cfg.scale;

  // mag = MinMaxFloat(36500.0, 2.1e6); // 2.1 million
  // mag = MinMaxFloat(3.65e4f, 2.1e6f); // 36.5 thousand, 2.1 million
  mag = MinMaxFloat(3.65e4f, 1.8e6f);

  cfg.mag.strong = 3.0;
  scale.factor = 2.41;
  scale.step = 0.01;

  auto scale_min = scaleVal(cfg.floor() * scale.factor);
  auto scale_max = scaleVal(cfg.ceiling());

  scale.minmax = MinMaxFloat(scale_min, scale_max);

  return cfg;
}

Config::Config(const Peak::Config &c) { *this = c; }

Config &Peak::Config::operator=(const Peak::Config &rhs) {

  mag.minmax = rhs.mag.minmax;
  scale.minmax = rhs.scale.minmax;
  scale.factor = rhs.scale.factor;
  scale.step = rhs.scale.step;

  return *this;
}
} // namespace peak

const MinMaxFloat Peak::magScaleRange() {
  MinMaxFloat range(0.0, _cfg.scaleCeiling() - _cfg.scaleFloor());
  return range;
}

using namespace peak;

Config Peak::_cfg = Peak::Config::defaults();

Peak::Peak(const size_t i, const Freq f, const Mag m) : _index(i), _freq(f), _mag(m) {}

MagScaled Peak::magScaled() const {
  auto scaled = scaleVal(magnitude());

  scaled -= _cfg.scaleFloor();
  if (scaled < 0) {
    scaled = 0;
  }

  return scaled;
}

Peak::operator bool() const {
  auto rc = false;

  if ((_mag > _cfg.floor()) && (_mag < _cfg.ceiling())) {
    rc = true;
  }

  return rc;
}

Peaks::Peaks(Peaks &&rhs) noexcept { *this = move(rhs); }

Peaks &Peaks::operator=(Peaks &&rhs) noexcept {
  // detect self assignment
  if (this != &rhs) {
    _peaks = move(rhs._peaks);
  }
  return *this;
}

bool Peaks::bass() const {
  bool bass = false;

  for (const Peak &peak : _peaks) {
    if (!peak.magStrong()) {
      break;
    } else if ((peak.frequency() > 30.0) && (peak.frequency() <= 170.0)) {
      bass = true;
      break;
    }
  }

  return bass;
}

bool Peaks::hasPeak(PeakN n) const {
  bool rc = false;

  // PeakN_t represents the peak of interest in the range of 1..max_peaks
  if (_peaks.size() && (_peaks.size() > n)) {
    rc = true;
  }

  return rc;
}

const Peak Peaks::majorPeak() const { return peakN(1); }

const Peak Peaks::peakN(const PeakN n) const {
  Peak peak;

  if (hasPeak(n)) {
    const Peak check = _peaks.at(n - 1);

    if (check.magnitude() > Peak::magFloor()) {
      peak = check;
    }
  }

  return peak;
}

void Peaks::sort() {
  std::sort(_peaks.begin(), _peaks.end(),
            [](const Peak &lhs, const Peak &rhs) { return lhs.magnitude() > rhs.magnitude(); });
}

} // namespace audio
} // namespace pierre
