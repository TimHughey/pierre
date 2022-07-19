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

#include "base/typical.hpp"

#include "dsp/peak.hpp"
#include "dsp/util.hpp"

#include <memory>
#include <type_traits>
#include <vector>

namespace pierre {

typedef size_t PeakN; // represents peak of interest 1..max_peaks

class Peaks;
typedef std::shared_ptr<Peaks> shPeaks;

class Peaks : public std::enable_shared_from_this<Peaks> {
public:
  static shPeaks create() { return shPeaks(new Peaks()); }
  ~Peaks() = default;

private:
  Peaks() = default;

public:
  bool hasPeak(PeakN n) const {
    // PeakN_t represents the peak of interest in the range of 1..max_peaks
    return _peaks.size() && (_peaks.size() > n) ? true : false;
  }

  const Peak majorPeak() const { return peakN(1); }

  static constexpr csv moduleID() { return module_id; }

  // find the first peak greater than the floating point value
  // specified in operator[]
  template <class T, class = typename std::enable_if<std::is_floating_point<T>::value>::type>
  const Peak operator[](T freq) {

    // only interested in the first five peaks
    auto found = ranges::find_if(ranges::take_view{_peaks, 5}, [freq = freq](Peak &peak) {
      return peak.greaterThanFreq(freq) ? true : false;
    });

    return found != _peaks.end() ? *found : Peak::zero();
  }

  const Peak peakN(const PeakN n) const {
    Peak peak;

    if (hasPeak(n)) {
      const Peak check = _peaks.at(n - 1);

      if (check.magnitude() > Peak::magFloor()) {
        peak = check;
      }
    }

    return peak;
  }

  void push_back(const Peak &peak) { _peaks.push_back(peak); }

  static bool silence(shPeaks peaks) {
    __LOGX(LCOL01 " use_count={} hasPeak={}\n", moduleID(), csv("SILENCE"), //
           peaks.use_count(), peaks.use_count() ? peaks->hasPeak(1) : "NA");

    return peaks.use_count() && peaks->hasPeak(1) ? true : false;
  }

  auto size() const { return _peaks.size(); }

  shPeaks sort();

private:
  std::vector<Peak> _peaks;

  static constexpr csv module_id = "PEAKS";
};

} // namespace pierre
