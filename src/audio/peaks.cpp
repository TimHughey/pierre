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

// #include <boost/format.hpp>    // only needed for printing
#include <fstream>
// #include <functional> // std::ref
#include <iostream>
#include <sstream> // std::ostringstream

#include "audio/peaks.hpp"
#include "misc/elapsed.hpp"

using namespace std;

namespace pierre {
namespace audio {

// Peak::Config Peak::_cfg{
//     .mag{.floor = 36500.0, .strong = 3.0, .ceiling = 1500000.0}};

Peak::Config Peak::_cfg = Peak::Config::defaults();

Peak::Config Peak::Config::defaults() {
  auto cfg = Config();

  auto &mag = cfg.mag.minmax;
  auto &scale = cfg.scale;

  mag = MinMaxFloat::make_shared(36500.0, 1500000.0);

  cfg.mag.strong = 3.0;
  scale.factor = 1.44;
  scale.step = 0.01;

  auto scale_min = Peak::scaleMagVal(cfg.floor() * scale.factor);
  auto scale_max = Peak::scaleMagVal(cfg.ceiling());

  scale.minmax = MinMaxFloat::make_shared(scale_min, scale_max);

  return std::move(cfg);
}

Peak::Config::Config(const Peak::Config &c) { *this = c; }

Peak::Config &Peak::Config::operator=(const Peak::Config &rhs) {

  mag.minmax = rhs.mag.minmax;
  scale.minmax = rhs.scale.minmax;
  scale.factor = rhs.scale.factor;
  scale.step = rhs.scale.step;

  return *this;
}

Peaks::Peaks() {
  auto buckets = Peak::config().ceiling() / (Peak::config().floor());
  _mag_histogram.assign(buckets + 1, 0);
}

void Peaks::analyzeMagnitudes() {
  // static ofstream log("/tmp/pierre/mags.csv", std::ios::trunc);
  static ofstream log("/dev/null", std::ios::trunc);
  static uint seq = 0;
  static elapsedMillis elapsed;

  uint overflow = 0;
  float mag_max = 0;
  auto buckets = Peak::config().ceiling() / (Peak::config().floor());

  if (_peaks.size() == 0) {
    return;
  }

  for (auto p = _peaks.cbegin(); p <= _peaks.cend(); p++) {
    auto threshold = Peak::config().floor() * 3.0;

    if (p->mag < threshold) {
      continue;
    }

    auto floor = Peak::config().floor();
    auto bucket = round(p->mag / floor);

    mag_max = std::max(mag_max, p->mag);

    if (bucket < buckets) {
      _mag_histogram[bucket]++;
    } else {
      overflow++;
    }
  }

  if (elapsed > (22 * 10)) {
    log << seq++ << "," << overflow;
    auto total = 0;
    for (auto k = _mag_histogram.cend(); k >= _mag_histogram.cbegin(); k--) {

      // log << ",";
      //
      // if (*k > 0) {
      //   log << *k;
      // } else {
      //   log << " ";
      // }

      total += *k;
    }

    log << "," << total;
    // log << "," << mag_max;
    log << endl;

    _mag_histogram.assign(buckets + 1, 0);
    overflow = 0;
    mag_max = 0.0;

    elapsed.reset();
  }
}

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
