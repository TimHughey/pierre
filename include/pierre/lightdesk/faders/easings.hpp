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
*/

#ifndef pierre_lightdesk_fader_easings_hpp
#define pierre_lightdesk_fader_easings_hpp

#include <cmath>

namespace pierre {
namespace lightdesk {
namespace fader {

constexpr double PI = 3.14159265358979323846;

struct EasingOutCirc {
  static float calc(float progress) {
    return sqrt(1.0 - pow(progress - 1.0, 2.0));
  }
  static const bool _easing = true;
};

struct EasingOutExponent {
  static float calc(float progress) {
    return (progress == 1.0 ? 1.0 : 1.0 - pow(2.0, -10.0 * progress));
  }
  static const bool _easing = true;
};

struct EasingOutQuint {
  static float calc(float progress) { return 1.0 - pow(1.0 - progress, 5.0); }
  static const bool _easing = true;
};

struct EasingOutSine {
  static float calc(float progress) { return sin((progress * PI) / 2.0); }
  static const bool _easing = true;
};

} // namespace fader
} // namespace lightdesk
} // namespace pierre

#endif
