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

#include "elapsed.hpp"

using namespace std::chrono;

namespace pierre {

elapsedMillis::elapsedMillis(void) { _ms = millis(); }

elapsedMillis::elapsedMillis(const elapsedMillis &orig) : _ms(orig._ms), _frozen(orig._frozen) {}

elapsedMillis &elapsedMillis::operator=(const elapsedMillis &rhs) {
  _ms = rhs._ms;
  return *this;
}

elapsedMillis &elapsedMillis::operator=(const uint32_t val) {
  _ms = millis() - val;
  return *this;
}

elapsedMillis &elapsedMillis::operator=(const int32_t val) {
  _ms = (val >= 0) ? millis() - (uint32_t)val : millis();
  return *this;
}

bool elapsedMillis::operator<(const int rhs) const {
  if (rhs >= 0) {
    return val() < (uint32_t)rhs;
  } else {
    return true;
  }
}

bool elapsedMillis::operator<=(const int rhs) const {
  if (rhs >= 0) {
    return (val() <= (uint32_t)rhs);
  } else {
    return false;
  }
}

bool elapsedMillis::operator>(const int rhs) const {
  if (rhs >= 0) {
    return val() > (uint32_t)rhs;
  } else {
    return false;
  }
}

bool elapsedMillis::operator>=(const int rhs) const {
  if (rhs >= 0) {
    return (val() >= (uint32_t)rhs);
  } else {
    return false;
  }
}

void elapsedMillis::freeze() {
  _frozen = true;
  _ms = millis() - _ms;
}

uint32_t elapsedMillis::millis() const {
  auto t = clock::now();

  auto ms = duration_cast<milliseconds>(t.time_since_epoch()).count();

  return ms;
}

void elapsedMillis::reset() {
  _frozen = false;
  _ms = millis();
}

float elapsedMillis::toSeconds() const { return (double)(millis() - _ms) / 1000.0; }

float elapsedMillis::toSeconds(uint32_t val) { return (double)val / 1000.0; }

//
// elapsedMicros
//

elapsedMicros::elapsedMicros(void) : _us(micros()) {}

elapsedMicros::elapsedMicros(const elapsedMicros &orig) : _us(orig._us), _frozen(orig._frozen) {}

elapsedMicros &elapsedMicros::operator=(const elapsedMicros &rhs) {
  _us = rhs._us;
  _frozen = rhs._frozen;
  return *this;
}

elapsedMicros &elapsedMicros::operator=(const uint32_t val) {
  _us = micros() - val;
  return *this;
}

void elapsedMicros::freeze() {
  _frozen = true;
  _us = micros() - _us;
}

uint64_t elapsedMicros::micros() const {
  auto t = clock::now();

  auto ms = duration_cast<microseconds>(t.time_since_epoch()).count();

  return ms;
}

void elapsedMicros::reset() {
  _frozen = false;
  _us = micros();
}

bool elapsedMicros::operator<(const int rhs) const {
  if (rhs >= 0) {
    return val() < (uint32_t)rhs;
  } else {
    return true;
  }
}

bool elapsedMicros::operator<=(const int rhs) const {
  if (rhs >= 0) {
    return (val() <= (uint32_t)rhs);
  } else {
    return false;
  }
}

bool elapsedMicros::operator>(const int rhs) const {
  if (rhs >= 0) {
    return val() > (uint32_t)rhs;
  } else {
    return false;
  }
}

bool elapsedMicros::operator>=(const int rhs) const {
  if (rhs >= 0) {
    return (val() >= (uint32_t)rhs);
  } else {
    return false;
  }
}

float elapsedMicros::toSeconds() const { return (double)(micros() - _us) / seconds_us; }

float elapsedMicros::toSeconds(uint32_t val) { return (double)val / seconds_us; }

} // namespace pierre
