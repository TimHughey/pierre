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

#pragma once

#include "base/types.hpp"

#include <cmath>

namespace pierre {
namespace fader {

struct Easing {
  Easing(const float step = 1.0f, const float start_val = 0.0f)
      : _step(step), _start_val(start_val) {}

  const bool easing = true;

  static constexpr double PI = 3.14159265358979323846;
  static constexpr double PI_HALF = (PI / 2.0);

protected:
  float _step = 1.0;
  float _start_val = 0.0;
};

struct Circular : public Easing {
  Circular(const float step = 1.0f, const float start_val = 0.0f) : Easing(step, start_val) {}

  float calc(float current, const float total) const;
};

struct CircularAcceleratingFromZero : public Easing {
  CircularAcceleratingFromZero(const float step = 1.0f, const float start_val = 0.0f)
      : Easing(step, start_val) {}

  float calc(float current, const float total) const;
};

struct CircularDeceleratingToZero : public Easing {
  CircularDeceleratingToZero(const float step = 1.0f, const float start_val = 0.0f)
      : Easing(step, start_val) {}

  float calc(float current, const float total) const;
};

struct Quadratic : public Easing {
  Quadratic(const float step = 1.0f, const float start_val = 0.0f) : Easing(step, start_val) {}

  float calc(float current, const float total) const;
};

struct QuintAcceleratingFromZero : public Easing {
  QuintAcceleratingFromZero(const float step = 1.0f, const float start_val = 0.0f)
      : Easing(step, start_val) {}

  float calc(float current, const float total) const;
};

struct QuintDeceleratingToZero : public Easing {
  QuintDeceleratingToZero(const float step = 1.0f, const float start_val = 0.0f)
      : Easing(step, start_val) {}

  float calc(float current, const float total) const;
};

struct SimpleLinear : public Easing {
  SimpleLinear(const float step = 1.0f, const float start_val = 0.0f) : Easing(step, start_val) {}

  float calc(const float current, const float total) const;
};

struct Sine : public Easing {
  Sine(const float step = 1.0f, const float start_val = 0.0f) : Easing(step, start_val) {}

  float calc(const float current, const float total) const;
};

struct SineAcceleratingFromZero : public Easing {
  SineAcceleratingFromZero(const float step = 1.0f, const float start_val = 0.0f)
      : Easing(step, start_val) {}

  float calc(const float current, const float total) const;
};

struct SineDeceleratingToZero : public Easing {
  SineDeceleratingToZero(const float step = 1.0f, const float start_val = 0.0f)
      : Easing(step, start_val) {}

  float calc(const float current, const float total) const;
};

} // namespace fader
} // namespace pierre
