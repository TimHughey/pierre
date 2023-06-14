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
#include "desk.hpp"
#include "unit/all.hpp"

#include <fmt/std.h>

namespace pierre {
namespace desk {

void Standby::apply_config() noexcept {
  INFO_AUTO_CAT("apply_config");

  tokc->table()->for_each( //
      overloaded{[&, this](const toml::key &key, const toml::value<double> &v) {
                   if (key == "hue_step") hue_step = Hue(*v);
                 },
                 [this](const toml::key &key, const toml::table &t) {
                   if (key == "color") {
                     if (auto c = Hsb(t); first_color != c) {
                       first_color = next_color = c;
                     }
                   }
                 }

      });

  save_silence_timeout(tokc->timeout_val("silence", Minutes(30)));
}

void Standby::execute(const Peaks &peaks) noexcept {
  INFO_AUTO_CAT("execute");

  if (tokc->changed()) {
    tokc->latest();
    apply_config();

    INFO_AUTO("{}", *tokc);
  }

  unit<PinSpot>(unit::MAIN_SPOT)->update(next_color);
  unit<PinSpot>(unit::FILL_SPOT)->update(next_color);

  next_color.rotate(hue_step);

  if (peaks.audible()) {
    next_fx = fx::MAJOR_PEAK;
    finished = true;
  }
}

bool Standby::once() noexcept {

  tokc->initiate_watch();

  unit<Switch>(unit::AC_POWER)->activate();
  unit(unit::DISCO_BALL)->dark();

  unit<Dimmable>(unit::EL_DANCE)->dim();
  unit<Dimmable>(unit::EL_DANCE)->dim();
  unit<Dimmable>(unit::LED_FOREST)->dim();

  return true;
}

} // namespace desk
} // namespace pierre
