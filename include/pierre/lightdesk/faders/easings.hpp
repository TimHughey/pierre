/*
    devs/pinspot/fader.hpp - Ruth Pin Spot Fader Action
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

    The Easings functions are copied from and/or inspired by:
    https:://www.wasings.net  (Andrey Sitnik and Ivan Solovev)
*/

#ifndef pierre_lightdesk_fader_easings_hpp
#define pierre_lightdesk_fader_easings_hpp

#include <cmath>

namespace pierre {
namespace lightdesk {
namespace fader {

struct Easing {
  bool in = false;
  bool out = false;
  bool easing = true;

  static constexpr double PI = 3.14159265358979323846;
};

struct EasingInOutExpo : public Easing {
  EasingInOutExpo() {
    in = true;
    out = true;
  }

  float calc(float progress) {
    if ((progress == 0.0) || (progress == 1.0)) {
      return progress;
    }

    if (progress < 0.5) {
      progress = pow(2.0, 20.0 * progress - 10.0) / 2.0;
    } else {
      progress = (2.0 - pow(2.0, -20.0 * progress + 10.0)) / 2.0;
    }

    return progress;
  }
};

struct EasingInQuint : public Easing {
  EasingInQuint() { in = true; }

  float calc(float progress) {
    return 1.0 - (progress * progress * progress * progress * progress);
  }
};

struct EasingInSine : public Easing {
  EasingInSine() { in = true; }

  float calc(float progress) { return 1.0 - sin((progress * PI) / 2.0); }
};

struct EasingOutCirc : public Easing {
  EasingOutCirc() { out = true; }

  float calc(float progress) { return sqrt(1.0 - pow(progress - 1.0, 2.0)); }
};

struct EasingOutExponent : public Easing {
  EasingOutExponent() { out = true; }

  float calc(float progress) {
    return (progress == 1.0 ? 1.0 : 1.0 - pow(2.0, -10.0 * progress));
  }
};

struct EasingOutQuint : public Easing {
  EasingOutQuint() { out = true; }

  float calc(float progress) { return pow(1.0 - progress, 5.0); }
};

struct EasingOutSine : public Easing {
  EasingOutSine() { out = true; }

  float calc(float progress) {
    out = true;
    return cos((progress * PI) / 2.0);
  }
};

} // namespace fader
} // namespace lightdesk
} // namespace pierre

#endif
