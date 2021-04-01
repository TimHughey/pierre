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

#ifndef pierre_lightdesk_fader_easings_hpp
#define pierre_lightdesk_fader_easings_hpp

#include <cmath>

namespace pierre {
namespace lightdesk {
namespace fader {

class Easing {
public:
  Easing(const float step = 1.0f, const float start_val = 0.0f)
      : _step(step), _start_val(start_val) {}

public:
  bool in = false;
  bool out = false;
  bool easing = true;

  static constexpr double PI = 3.14159265358979323846;
  static constexpr double PI_HALF = (PI / 2.0);

protected:
  float _step = 1.0;
  float _start_val = 0.0;
};

// class EasingInOutExpo : public Easing {
// public:
//   EasingInOutExpo() {
//     in = true;
//     out = true;
//   }
//
//   float calc(float progress) const;
// };
//
// class EasingInQuint : public Easing {
// public:
//   EasingInQuint() { in = true; }
//
//   float calc(float progress) const;
// };
//
// class EasingInSine : public Easing {
// public:
//   EasingInSine() { in = true; }
//
//   float calc(float progress) const;
// };
//
// class EasingOutCirc : public Easing {
// public:
//   EasingOutCirc() { out = true; }
//
//   float calc(float progress) const;
// };
//
// class EasingOutExponent : public Easing {
// public:
//   EasingOutExponent() { out = true; }
//
//   float calc(float progress) const;
// };
//
// class EasingOutQuint : public Easing {
// public:
//   EasingOutQuint() { out = true; }
//
//   float calc(float progress) const;
// };
//
// class EasingOutSine : public Easing {
// public:
//   EasingOutSine(const float step, const float start_val)
//       : Easing(step, start_val) {
//     out = true;
//   }
//
//   float calc(float progress) const;
//   float calc2(const float total, const float current) const;
// };

class AcceleratingFromZeroSine : public Easing {
public:
  AcceleratingFromZeroSine(const float step = 1.0f,
                           const float start_val = 0.0f)
      : Easing(step, start_val) {
    out = true;
  }

  float calc(const float current, const float total) const;
};

class DeceleratingToZeroSine : public Easing {
public:
  DeceleratingToZeroSine(const float step = 1.0f, const float start_val = 0.0f)
      : Easing(step, start_val) {
    out = true;
  }

  float calc(const float current, const float total) const;
};

} // namespace fader
} // namespace lightdesk
} // namespace pierre

#endif
