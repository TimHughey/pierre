//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include "desk/fx/standby.hpp"
#include "base/conf/token.hpp"
#include "base/dura_t.hpp"
#include "base/logger.hpp"
#include "color.hpp"
#include "desk.hpp"
#include "unit/all.hpp"

#include <fmt/std.h>

namespace pierre {
namespace desk {

void Standby::apply_config() noexcept {
  INFO_AUTO_CAT("apply_config");

  auto cfg_first_color = Color({.hue = tokc->val<double>("color.hue"_tpath, 0.0),
                                .sat = tokc->val<double>("color.sat"_tpath, 0.0),
                                .bri = tokc->val<double>("color.bri"_tpath, 0.0)});

  if (cfg_first_color != first_color) {
    std::exchange(first_color, cfg_first_color);
    next_color = first_color;
    next_brightness = 0.0;
    max_brightness = next_color.brightness();
  }

  // hue_step = config_val<double>(ctoken, "hue_step", 0.0);
  hue_step = tokc->val<double>("hue_step"_tpath, 0.0);

  INFO_AUTO("hue_step={}", hue_step);
  set_silence_timeout(tokc, fx::ALL_STOP, Minutes(30));
}

void Standby::execute(const Peaks &peaks) noexcept {
  INFO_AUTO_CAT("execute");

  if (tokc->changed()) {
    tokc->latest();
    apply_config();

    INFO_AUTO("{}", *tokc);
  }

  if (next_brightness < max_brightness) {
    next_brightness++;
    next_color.setBrightness(next_brightness);
  }

  get_unit<PinSpot>(unit::MAIN_SPOT)->colorNow(next_color);
  get_unit<PinSpot>(unit::FILL_SPOT)->colorNow(next_color);

  if (next_brightness >= max_brightness) next_color.rotateHue(hue_step);

  set_finished(peaks.audible(), fx::MAJOR_PEAK);
}

bool Standby::once() noexcept {

  tokc->initiate_watch();

  get_unit<Switch>(unit::AC_POWER)->activate();
  get_unit(unit::DISCO_BALL)->dark();

  get_unit<Dimmable>(unit::EL_DANCE)->dim();
  get_unit<Dimmable>(unit::EL_DANCE)->dim();
  get_unit<Dimmable>(unit::LED_FOREST)->dim();

  return true;
}

} // namespace desk
} // namespace pierre
