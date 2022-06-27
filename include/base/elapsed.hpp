/*
    Pierre - Custom Lightshow for Wiss Landing
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

#pragma once

#include <chrono>
#include <cstdint>

namespace pierre {
class elapsedMillis {
public:
  typedef std::chrono::steady_clock clock;
  typedef std::chrono::time_point<clock> timepoint;
  typedef std::chrono::milliseconds milliseconds;

public:
  elapsedMillis(void) { _ms = millis(); }
  elapsedMillis(const elapsedMillis &orig) : _ms(orig._ms), _frozen(orig._frozen) {}

  operator uint32_t() const { return val(); }
  operator float() const { return toSeconds(val()); }
  elapsedMillis &operator=(const elapsedMillis &rhs) {
    _ms = rhs._ms;
    return *this;
  }
  elapsedMillis &operator=(const uint32_t val) {
    _ms = millis() - val;
    return *this;
  }
  elapsedMillis &operator=(const int32_t val) {
    _ms = (val >= 0) ? millis() - (uint32_t)val : millis();
    return *this;
  }

  bool operator<(const elapsedMillis &rhs) const { return val() < rhs.val(); }
  bool operator<(const uint32_t rhs) const { return val() < rhs; }
  bool operator<=(const uint32_t rhs) const { return val() <= rhs; }

  bool operator<(const int rhs) const { return rhs >= 0 ? val() < (uint32_t)rhs : true; }
  bool operator<=(const int rhs) const { return rhs >= 0 ? val() <= (uint32_t)rhs : false; }
  bool operator>(const uint32_t rhs) const { return val() > rhs; }
  bool operator>=(const uint32_t rhs) const { return val() >= rhs; }

  bool operator>(const int rhs) const { return rhs >= 0 ? val() > (uint32_t)rhs : false; }
  bool operator>=(const int rhs) const { return rhs > 0 ? val() >= (uint32_t)rhs : false; }

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
    auto t = clock::now();

    auto ms = duration_cast<milliseconds>(t.time_since_epoch()).count();

    return ms;
  }
  inline uint32_t val() const { return (_frozen) ? (_ms) : (millis() - _ms); }
};

class elapsedMicros {
public:
  typedef std::chrono::steady_clock clock;
  typedef std::chrono::time_point<clock> timepoint;
  typedef std::chrono::microseconds microseconds;

public:
  elapsedMicros(void) : _us(micros()) {}
  elapsedMicros(const elapsedMicros &rhs) : _us(rhs._us), _frozen(rhs._frozen) {}
  float asMillis() const { return val() / 1000.0; }
  operator float() const { return toSeconds(val()); }
  operator uint32_t() const { return val(); }

  elapsedMicros &operator=(const elapsedMicros &rhs) {
    _frozen = rhs._frozen;
    _us = rhs._us;
  }

  elapsedMicros &operator=(const uint32_t val) {
    _us = (val >= 0) ? micros() - (uint32_t)val : micros();
    return *this;
  }

  void freeze() {
    _frozen = true;
    _us = micros() - _us;
  }

  void reset() {
    _frozen = false;
    _us = micros();
  }

  bool operator<(const elapsedMicros &rhs) const { return val() < rhs.val(); }
  bool operator<(const uint32_t rhs) const { return val() < rhs; }
  bool operator<(const int rhs) const {
    if (rhs >= 0) {
      return val() < (uint32_t)rhs;
    } else {
      return true;
    }
  }
  bool operator<=(const uint32_t rhs) const { return val() <= rhs; }
  bool operator<=(const int rhs) const {
    if (rhs >= 0) {
      return (val() <= (uint32_t)rhs);
    } else {
      return false;
    }
  }

  bool operator>(const elapsedMicros &rhs) const { return val() > rhs.val(); }
  bool operator>(uint32_t rhs) const { return val() > rhs; }
  bool operator>(const int rhs) const {
    if (rhs >= 0) {
      return (val() <= (uint32_t)rhs);
    } else {
      return false;
    }
  }
  bool operator>=(uint32_t rhs) const { return val() >= rhs; }
  bool operator>=(const int rhs) const { return rhs >= 0 ? (val() >= (uint32_t)rhs) : false; }

  float toSeconds() const { return (double)(micros() - _us) / seconds_us; }
  static float toSeconds(uint32_t val) { return (double)val / seconds_us; }

private:
  uint32_t _us;
  bool _frozen = false;

  static constexpr double seconds_us = 1000.0 * 1000.0;

  uint64_t micros() const {
    return duration_cast<microseconds>(clock::now().time_since_epoch()).count();
  }

  inline uint32_t val() const { return (_frozen) ? (_us) : (micros() - _us); }
};

} // namespace pierre
