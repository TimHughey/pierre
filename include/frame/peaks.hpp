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

#include "frame/peak.hpp"

#include <memory>
#include <type_traits>
#include <vector>

namespace pierre {

using peak_n_t = size_t; // represents peak of interest 1..max_peaks

class Peaks;
using peaks_t = std::shared_ptr<Peaks>;

class Peaks : public std::enable_shared_from_this<Peaks> {
public:
  static peaks_t create() { return std::shared_ptr<Peaks>(new Peaks()); }
  ~Peaks() = default;

private:
  Peaks() = default;

public:
  template <class... Args> void emplace_back(Args &&...args) { _peaks.emplace_back(args...); }

  bool hasPeak(peak_n_t n) const {
    // peak_n_t_t represents the peak of interest in the range of 1..max_peaks
    return _peaks.size() > n ? true : false;
  }

  const Peak majorPeak() const { return peak_n(1); }

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

  const Peak peak_n(const peak_n_t n) const {
    Peak peak;

    if (hasPeak(n)) {
      const Peak check = _peaks.at(n - 1);

      if (check.magnitude() > Peak::mag_base.floor) {
        peak = check;
      }
    }

    return peak;
  }

  static bool silence(peaks_t peaks) {
    __LOGX(LCOL01 " use_count={} hasPeak={}\n", module_id, csv("SILENCE"), //
           peaks.use_count(), peaks.use_count() ? peaks->hasPeak(1) : false);

    return (peaks && peaks->hasPeak(1)) == false;
  }

  auto size() const { return _peaks.size(); }

  peaks_t sort();

private:
  std::vector<Peak> _peaks;

public:
  static constexpr csv module_id = "PEAKS";
};

} // namespace pierre
