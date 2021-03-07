/*
    misc/derivative.hpp -- Derivative (rate of change)
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

#ifndef pierre_derivative_hpp
#define pierre_derivative_hpp

#include <cstdint>
#include <vector>

#include "local/types.hpp"

namespace pierre {

template <typename T, size_t CAP = 5> class Derivative {
  using vector = std::vector<T>;

public:
  Derivative() {
    _points.resize(CAP);
    _associated_value.resize(CAP);
  };

  void addValue(T val, T associated_value) {

    // the _points vector contains a history of values constrained to the
    // initial capacity. the values are ordered by time with the latest
    // at the end and the oldest at the front.
    if (_points.size() < CAP) {
      // simply add values to _points until it reaches capacity
      _points.push_back(val);
      _associated_value.push_back(associated_value);
    } else {
      // once capacity is reached, remove the first value then add the latest
      // to the end to prevent realloaction / resizing.
      _points.erase(_points.begin());
      _points.push_back(val);

      _associated_value.erase(_associated_value.begin());
      _associated_value.push_back(associated_value);
      calculate();
    }
  }

  bool calculated() const { return _calculated; }

  // return the current rate of change and the value associated with the
  // rate of change (now/2)
  T rateOfChange(T &val, T &associated_value) {
    val = 0;
    associated_value = 0;

    if (_calculated) {
      associated_value = _associated_value[2];
      val = _points[2];
      return _rate_of_change;
    } else {
      return 0;
    }
  }

private:
  void calculate() {
    _rate_of_change = 0;

    for (auto k = 0; k < _num_coeff; k++) {
      _rate_of_change += _points[k] * static_cast<T>(_coeff[k]);
    }

    _rate_of_change /= CAP;
    _calculated = true;
  }

private:
  vector _points = {1};
  vector _associated_value = {};

  const int_fast16_t _coeff[5] = {1, -8, 0, 8, -1};
  static constexpr size_t _num_coeff = sizeof(_coeff) / sizeof(int_fast16_t);

  bool _calculated = false;

  T _rate_of_change;
};

} // namespace pierre
#endif
