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
  elapsedMillis(void);
  elapsedMillis(const elapsedMillis &orig);

  operator uint32_t() const { return val(); }
  operator float() const { return toSeconds(val()); }
  elapsedMillis &operator=(const elapsedMillis &rhs);
  elapsedMillis &operator=(const uint32_t val);
  elapsedMillis &operator=(const int32_t val);

  bool operator<(const elapsedMillis &rhs) const { return val() < rhs.val(); }
  bool operator<(const uint32_t rhs) const { return val() < rhs; }
  bool operator<=(const uint32_t rhs) const { return val() <= rhs; }

  bool operator<(const int rhs) const;
  bool operator<=(const int rhs) const;

  bool operator>(const uint32_t rhs) const { return val() > rhs; }
  bool operator>=(const uint32_t rhs) const { return val() >= rhs; }

  bool operator>(const int rhs) const;
  bool operator>=(const int rhs) const;

  void freeze();
  void reset();

  float toSeconds() const;
  static float toSeconds(uint32_t val);

private:
  uint32_t _ms;
  bool _frozen = false;

  uint32_t millis() const;
  inline uint32_t val() const { return (_frozen) ? (_ms) : (millis() - _ms); }
};

class elapsedMicros {

public:
  typedef std::chrono::steady_clock clock;
  typedef std::chrono::time_point<clock> timepoint;
  typedef std::chrono::microseconds microseconds;

public:
  elapsedMicros(void);
  elapsedMicros(const elapsedMicros &orig);

  float asMillis() const { return val() / 1000.0; }
  operator float() const { return toSeconds(val()); }
  operator uint32_t() const { return val(); }

  elapsedMicros &operator=(const elapsedMicros &rhs);
  elapsedMicros &operator=(const uint32_t val);

  void freeze();
  void reset();

  bool operator<(const elapsedMicros &rhs) const { return val() < rhs.val(); }
  bool operator<(const uint32_t rhs) const { return val() < rhs; }
  bool operator<(const int rhs) const;
  bool operator<=(const uint32_t rhs) const { return val() <= rhs; }
  bool operator<=(const int rhs) const;

  bool operator>(const elapsedMicros &rhs) const { return val() > rhs.val(); }
  bool operator>(uint32_t rhs) const { return val() > rhs; }
  bool operator>(const int rhs) const;
  bool operator>=(uint32_t rhs) const { return val() >= rhs; }
  bool operator>=(const int rhs) const;

  float toSeconds() const;
  static float toSeconds(uint32_t val);

private:
  uint32_t _us;
  bool _frozen = false;

  static constexpr double seconds_us = 1000.0 * 1000.0;

  uint64_t micros() const;

  inline uint32_t val() const { return (_frozen) ? (_us) : (micros() - _us); }
};

} // namespace pierre
