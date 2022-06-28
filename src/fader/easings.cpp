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

    The Easings functions are copied from and/or inspired by:
    https:://www.wasings.net  (Andrey Sitnik and Ivan Solovev)
*/

#include "fader/easings.hpp"

#include <cmath>

namespace pierre {
namespace fader {

float Circular::calc(float current, const float total) const {
  float x = 0.0f;

  current /= total / 2.0f;
  if (current < 1) {
    x = copysign(_step, -1.0f) / 2.0f * (sqrt(1.0f - current * current) - 1.0f) + _start_val;
  } else {
    current -= 2.0f;
    x = _step / 2 * (sqrt(1.0f - current * current) + 1.0f) + _start_val;
  }

  return x;
}

float CircularAcceleratingFromZero::calc(float current, const float total) const {
  current /= total;
  const float x = copysign(_step, -1.0f) * (sqrt(1.0f - current * current) - 1) + _start_val;

  return x;
}

float CircularDeceleratingToZero::calc(float current, const float total) const {
  current /= total;
  --current;

  const float x = _step * (sqrt(1.0f - current * current)) + _start_val;

  return x;
}

float Quadratic::calc(float current, const float total) const {
  float x = 0.0;

  current /= total / 2.0f;
  if (current < 1.0f) {
    x = _step / 2.0f * current * current + _start_val;
  } else {
    current--;
    x = copysign(_step, -1.0f) / 2.0f * (current * (current - 2.0f) - 1.0f) + _start_val;
  }

  return x;
}

float QuintAcceleratingFromZero::calc(float current, const float total) const {
  current /= total;

  const float x = _step * pow(current, 5.0f) + _start_val;

  return x;
}

float QuintDeceleratingToZero::calc(float current, const float total) const {
  current /= total;
  current--;

  const float x = _step * (pow(current, 5.0f) + 1.0f) + _start_val;

  return x;
}

float SimpleLinear::calc(const float current, const float total) const {
  const float x = _step * current / total + _start_val;

  return x;
}

float Sine::calc(const float current, const float total) const {
  const float x = copysign(_step, -1.0f) / 2.0f * (cos(PI * current / total) - 1.0f) + _start_val;

  return x;
}

float SineAcceleratingFromZero::calc(const float current, const float total) const {
  const float x = _step * sin((current / total) * PI_HALF) + _start_val;

  return x;
}

float SineDeceleratingToZero::calc(const float current, const float total) const {
  const float x = copysign(_step, -1.0f) * cos((current / total) * PI_HALF) + _step + _start_val;

  return x;
}

} // namespace fader
} // namespace pierre
