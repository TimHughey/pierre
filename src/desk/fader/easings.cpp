
// Pierre - Custom Light Show via DMX for Wiss Landing
// Copyright (C) 2021  Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com
//
// The Easings functions are copied from and/or inspired by
// Andrey Sitnik and Ivan Solovev https://www.easings.net

#include "fader/easings.hpp"

#include <cmath>

namespace pierre {
namespace desk {
namespace fader {

/*

double CircularAcceleratingFromZero::calc(double current, double total) const {
  current /= total;
  const double x = copysign(_step, -1.0f) * (sqrt(1.0f - current * current) - 1) + _start_val;

  return x;
}

double CircularDeceleratingToZero::calc(double current, double total) const {
  current /= total;
  --current;

  const double x = _step * (sqrt(1.0f - current * current)) + _start_val;

  return x;
}

double Quadratic::calc(double current, double total) const {
  double x = 0.0;

  current /= total / 2.0f;
  if (current < 1.0f) {
    x = _step / 2.0f * current * current + _start_val;
  } else {
    current--;
    x = copysign(_step, -1.0f) / 2.0f * (current * (current - 2.0f) - 1.0f) + _start_val;
  }

  return x;
}

double QuintAcceleratingFromZero::calc(double current, double total) const {
  current /= total;

  const double x = _step * pow(current, 5.0f) + _start_val;

  return x;
}

double QuintDeceleratingToZero::calc(double current, double total) const {
  current /= total;
  current--;

  const double x = _step * (pow(current, 5.0f) + 1.0f) + _start_val;

  return x;
}*/

/*double Sine::calc(const double current, double total) const {
  const double x = copysign(_step, -1.0f) / 2.0f * (cos(PI * current / total) - 1.0f) + _start_val;

  return x;
}

double SineAcceleratingFromZero::calc(const double current, double total) const {
  const double x = _step * sin((current / total) * PI_HALF) + _start_val;

  return x;
}

double SineDeceleratingToZero::calc(const double current, double total) const {
  const double x = copysign(_step, -1.0f) * cos((current / total) * PI_HALF) + _step + _start_val;

  return x;
}*/

} // namespace fader
} // namespace desk
} // namespace pierre
