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
#include "base/pet_types.hpp"
#include "color.hpp"
#include "desk.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "unit/all.hpp"

namespace pierre {
namespace desk {

void Standby::apply_config() noexcept {
  static constexpr csv fn_id{"apply_config"};

  // auto cfg_first_color = Color({.hue = config_val<double>(ctoken, "color.hue"sv, 0),
  //                               .sat = config_val<double>(ctoken, "color.sat"sv, 0),
  //                               .bri = config_val<double>(ctoken, "color.bri"sv, 0)});

  auto cfg_first_color = Color({.hue = ctoken.val<double>("color.hue"sv, 0.0),
                                .sat = ctoken.val<double>("color.sat"sv, 0.0),
                                .bri = ctoken.val<double>("color.bri"sv, 0.0)});

  if (cfg_first_color != first_color) {
    std::exchange(first_color, cfg_first_color);
    next_color = first_color;
    next_brightness = 0.0;
    max_brightness = next_color.brightness();
  }

  // hue_step = config_val<double>(ctoken, "hue_step", 0.0);
  hue_step = ctoken.val<double>("hue_step"sv, 0.0);

  const auto timeout = ctoken.val<Minutes>(cfg_silence_timeout, 30);
  set_silence_timeout(timeout, fx::ALL_STOP);
}

void Standby::execute(const Peaks &peaks) noexcept {
  static constexpr csv fn_id{"execute"};

  if (next_brightness < max_brightness) {
    next_brightness++;
    next_color.setBrightness(next_brightness);
  }

  units.get<PinSpot>(unit::MAIN_SPOT)->colorNow(next_color);
  units.get<PinSpot>(unit::FILL_SPOT)->colorNow(next_color);

  if (next_brightness >= max_brightness) next_color.rotateHue(hue_step);

  set_finished(peaks.audible(), fx::MAJOR_PEAK);
}

void Standby::once() noexcept {

  units(unit::AC_POWER)->activate();
  units(unit::DISCO_BALL)->dark();

  units.get<Dimmable>(unit::EL_DANCE)->dim();
  units.get<Dimmable>(unit::EL_DANCE)->dim();
  units.get<Dimmable>(unit::LED_FOREST)->dim();
}

void Standby::register_token(config2::token::lambda &&handler) noexcept {

  config_watch().register_token(ctoken, std::move(handler));
}

} // namespace desk
} // namespace pierre
