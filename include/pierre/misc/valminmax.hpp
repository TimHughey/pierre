/*
    misc/elapsed.hpp - Ruth Value Min, Max and Current Tracking
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

#ifndef pierre_misc_valminmax_hpp
#define pierre_misc_valminmax_hpp

#include <algorithm>
#include <limits>

#include "local/types.hpp"

namespace pierre {

template <typename T> class ValMinMax {
public:
  ValMinMax() {}
  ValMinMax(uint32_t reset_ms) : _reset_ms(reset_ms) {}

  inline void track(const T val) {
    _current = val;
    _max = std::max(val, _max);
    _min = std::min(val, _min);
  }

  inline void reset() {
    _max = type_limit::min();
    _min = type_limit::max();
    _current = 0;
  }

  inline const T current() const { return _current; }
  inline const T max() const { return _max; }
  inline const T min() const { return _min; }

private:
  typedef std::numeric_limits<T> type_limit;

  T _max = type_limit::min();
  T _min = type_limit::max();
  T _current = 0;

  uint32_t _reset_ms = 0;

  // elapsedMillis _e;
};

typedef class ValMinMax<float> ValMinMaxFloat_t;

} // namespace pierre

#endif
