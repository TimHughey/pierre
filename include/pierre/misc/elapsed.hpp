/*
    misc/elapsed.hpp - Ruth Elapsed Time Measurement
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

    Based on the original work of:
      http://www.pjrc.com/teensy/
      Copyright (c) 2011 PJRC.COM, LLC
*/

#ifndef pierre_misc_elapsed_hpp
#define pierre_misc_elapsed_hpp

#include <cstdint>

#include <time.h>

namespace pierre {
class elapsedMillis {

public:
  elapsedMillis(void) { _ms = millis(); }
  elapsedMillis(const elapsedMillis &orig)
      : _ms(orig._ms), _frozen(orig._frozen) {}

  operator uint32_t() const { return val(); }
  // operator uint32_t() const { return (uint32_t)val(); }
  operator float() const { return toSeconds(val()); }
  elapsedMillis &operator=(const elapsedMillis &rhs) {
    _ms = rhs._ms;
    return *this;
  }

  elapsedMillis &operator=(uint32_t val) {
    _ms = millis() - val;
    return *this;
  }

  bool operator<(const elapsedMillis &rhs) const { return val() < rhs.val(); }
  bool operator<(uint32_t rhs) const { return (val() < rhs) ? true : false; }
  bool operator<=(uint32_t rhs) const { return (val() <= rhs) ? true : false; }
  // bool operator<(uint32_t rhs) const { return (val() < rhs) ? true : false; }
  // bool operator<=(uint32_t rhs) const { return (val() <= rhs) ? true : false; }
  bool operator<(int rhs) const { return (val() < rhs) ? true : false; }
  bool operator<=(int rhs) const { return (val() <= rhs) ? true : false; }

  bool operator>(uint32_t rhs) const { return (val() > rhs) ? true : false; }
  bool operator>=(uint32_t rhs) const { return (val() >= rhs) ? true : false; }
  // bool operator>(uint32_t rhs) const { return (val() > rhs) ? true : false; }
  // bool operator>=(uint32_t rhs) const { return (val() >= rhs) ? true : false;
  // }
  bool operator>(int rhs) const { return (val() > rhs) ? true : false; }
  bool operator>=(int rhs) const { return (val() >= rhs) ? true : false; }

  void freeze() {
    _frozen = true;
    _ms = millis() - _ms;
  }

  void reset() {
    _frozen = false;
    _ms = millis();
  }

  float toSeconds() const { return (double)(millis() - _ms) / 1000.0; }
  static float toSeconds(uint32_t val) { return (double)val / 1000.0; }

private:
  uint32_t _ms;
  bool _frozen = false;

  uint32_t millis() const {
    struct timespec t;
    clock_gettime(CLOCK_BOOTTIME, &t);

    auto ms = t.tv_nsec / (1000 * 1000);

    return ms;
  };
  inline uint32_t val() const { return (_frozen) ? (_ms) : (millis() - _ms); }
};

class elapsedMicros {

public:
  elapsedMicros(void) {
    _us = micros();
    _frozen = false;
  }

  elapsedMicros(const elapsedMicros &orig) {
    _us = orig._us;
    _frozen = orig._frozen;
  }

  float asMillis() const { return double(val()) / 1000.0; }

  operator float() const { return toSeconds(val()); }
  operator uint32_t() const { return val(); }
  // operator uint32_t() const { return (uint32_t)val(); }
  elapsedMicros &operator=(const elapsedMicros &rhs) {
    _us = rhs._us;
    _frozen = rhs._frozen;
    return *this;
  }
  elapsedMicros &operator=(uint32_t val) {
    _us = micros() - val;
    return *this;
  }

  inline void freeze() {
    _frozen = true;
    _us = micros() - _us;
  }
  inline void reset() {
    _frozen = false;
    _us = micros();
  }

  bool operator>(const elapsedMicros &rhs) const { return val() > rhs.val(); }
  bool operator>(int rhs) const { return (val() > rhs) ? true : false; }
  bool operator>=(int rhs) const { return (val() >= rhs) ? true : false; }
  bool operator>(uint32_t rhs) const { return (val() > rhs) ? true : false; }
  bool operator>=(uint32_t rhs) const { return (val() >= rhs) ? true : false; }
  // bool operator>(uint32_t rhs) const { return (val() > rhs) ? true : false; }
  // bool operator>=(uint32_t rhs) const { return (val() >= rhs) ? true : false;
  // }

  bool operator<(const elapsedMicros &rhs) const { return val() < rhs.val(); }
  bool operator<(int rhs) const { return (val() < rhs) ? true : false; }
  bool operator<=(int rhs) const { return (this->val() <= rhs) ? true : false; }
  bool operator<(uint32_t rhs) const { return (val() < rhs) ? true : false; }
  bool operator<=(uint32_t rhs) const { return (val() <= rhs) ? true : false; }
  // bool operator<(uint32_t rhs) const { return (val() < rhs) ? true : false; }

  float toSeconds() const { return (double)(micros() - _us) / seconds_us; }
  static float toSeconds(uint32_t val) { return (double)val / seconds_us; }

private:
  uint32_t _us;
  bool _frozen = false;

  static constexpr double seconds_us = 1000.0 * 1000.0;

  uint32_t micros() const {
    struct timespec t;
    clock_gettime(CLOCK_BOOTTIME, &t);

    auto us = t.tv_nsec / 1000;

    return us;
  }

  inline uint32_t val() const { return (_frozen) ? (_us) : (micros() - _us); }
};

} // namespace pierre

#endif
