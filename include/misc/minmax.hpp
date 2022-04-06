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

#include <algorithm>
#include <memory>
#include <tuple>

namespace pierre {

template <typename T> class MinMaxPair {
public:
  MinMaxPair() { set(0, 100); }
  MinMaxPair(const T min_val, const T max_val) { _pair = std::make_pair(min_val, max_val); }

  static MinMaxPair<T> defaults() {
    auto x = MinMaxPair<T>(0, 100);

    return std::move(x);
  }

  const T interpolate(const MinMaxPair<T> &bpair, const T val) const {
    // https://stackoverflow.com/questions/929103/convert-a-number-range-to-another-range-maintaining-ratio
    // OldRange = (OldMax - OldMin)
    // NewRange = (NewMax - NewMin)
    // NewValue = (((OldValue - OldMin) * NewRange) / OldRange) + NewMin

    const T range_a = max() - min();
    const T range_b = bpair.max() - bpair.min();

    const T x = (((val - min()) * range_b) / range_a) + bpair.min();

    return x;
  }

  const T &max() const { return std::get<1>(_pair); }

  // MinMaxPair<T> &operator=(std::shared_ptr<MinMaxPair<T>> rhs) {
  //   set(rhs);
  //   return *this;
  // }

  const T &min() const { return std::get<0>(_pair); }

  void set(const T min_val, const T max_val) {
    _pair.first = min_val;
    _pair.second = max_val;
  }

  // void set(std::shared_ptr<MinMaxPair<T>> obj) {
  //   _pair = std::make_pair(obj->min(), obj->max());
  // }

private:
  std::pair<T, T> _pair;
};

typedef MinMaxPair<float> MinMaxFloat;

} // namespace pierre
