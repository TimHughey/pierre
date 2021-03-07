/*
    misc/tracked.hpp - Ruth Tracking Objects
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

#ifndef pierre_misc_tracked_hpp
#define pierre_misc_tracked_hpp

#include <algorithm>
#include <limits>

#include "local/types.hpp"
#include "misc/valminmax.hpp"

namespace pierre {

class CountPerInterval {
public:
  CountPerInterval(uint_fast32_t interval_ms = 1000)
      : _interval_us(interval_ms * 1000.0f) {}

  // support externally driven calculation triggers
  void calculate() { track(0); }

  void reset() {
    _count = 0;
    // _e.reset();
  }

  void track(uint_fast32_t amount = 1) {
    if (_first_track) {
      // _e.reset();
      _count = amount;
    }

    //   if (_e <= (uint_fast32_t)_interval_us) {
    //     _count += amount;
    //   } else {
    //     const float us = (uint32_t)_e;
    //     const float per_interval = (float)_count / (us / _interval_us);
    //
    //     _e.reset();
    //     _v.track(per_interval);
    //
    //     _count = amount;
    //   }
    //
    //   _first_track = false;
  }

  float current() const { return _v.current(); }
  float max() const { return _v.max(); }
  float min() const { return _v.min(); }

private:
  float _interval_us = 0;
  uint_fast32_t _count = 0;
  bool _first_track = true;

  // elapsedMicros _e;
  ValMinMaxFloat_t _v;
};

//
// Tracked elapsed milliseconds
//

class elapsedMillisTracked {
public:
  elapsedMillisTracked() {}

  inline void track() {
    // _e.freeze();
    // _v.track(_e);
  }

  inline void reset() {
    // _e.reset();
  }

  // value min, current, max access
  uint_fast32_t current() const { return 1; }
  uint_fast32_t min() const { return 1; }
  uint_fast32_t max() const { return 1; }

private:
  // elapsedMillis _e;
  // ValMinMax<uint_fast32_t> _v;
};

class elapsedMicrosTracked {
public:
  elapsedMicrosTracked() {}

  inline void record() {
    // _e.freeze();
    // _v.track(_e);
  }

  inline void track() { // _e.reset();
  }

  // value min, current, max access
  uint_fast64_t current() const { return 1; }
  uint_fast64_t min() const { return 1; }
  uint_fast64_t max() const { return 1; }

  float currentAsMillis() const { return 1000.0 / 1000.0; }
  float minAsMillis() const { return 1000.0 / 1000.0; }
  float maxAsMillis() const { return 1000.0 / 1000.0; }

private:
  // elapsedMicros _e;
  // ValMinMax<uint_fast64_t> _v;
};

} // namespace pierre

#endif
