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

#include <cmath>

#include "lightdesk/faders/easings.hpp"

namespace pierre {
namespace lightdesk {
namespace fader {

// float EasingInOutExpo::calc(float progress) const {
//   if ((progress == 0.0) || (progress == 1.0)) {
//     return progress;
//   }
//
//   if (progress < 0.5) {
//     progress = pow(2.0, 20.0 * progress - 10.0) / 2.0;
//   } else {
//     progress = (2.0 - pow(2.0, -20.0 * progress + 10.0)) / 2.0;
//   }
//
//   return progress;
// }
//
// float EasingInQuint::calc(float progress) const {
//   return 1.0 - (progress * progress * progress * progress * progress);
// }
//
// float EasingInSine::calc(float progress) const {
//   return 1.0 - sin((progress * PI) / 2.0);
// }
//
// float EasingOutCirc::calc(float progress) const {
//   return sqrt(1.0 - pow(progress - 1.0, 2.0));
// }
//
// float EasingOutExponent::calc(float progress) const {
//   return (progress == 1.0 ? 1.0 : 1.0 - pow(2.0, -10.0 * progress));
// }
//
// float EasingOutQuint::calc(float progress) const {
//   return pow(1.0 - progress, 5.0);
// }
//
// float EasingOutSine::calc(float progress) const {
//   return cos((progress * PI) / 2.0);
// }

float AcceleratingFromZeroSine::calc(float current, float total) const {
  float x = _step * sin((current / total) * PI_HALF) + _start_val;

  return x;
}

float DeceleratingToZeroSine::calc(float current, float total) const {
  float x = copysign(_step, -1.0f) * cos((current / total) * PI_HALF) + _step +
            _start_val;

  return x;
}

} // namespace fader
} // namespace lightdesk
} // namespace pierre
