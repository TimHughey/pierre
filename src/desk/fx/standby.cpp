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
#include "color.hpp"
#include "config/config.hpp"
#include "desk.hpp"
#include "unit/all.hpp"

namespace pierre {
namespace fx {

void Standby::execute(Peaks &peaks) {
  if (cfg_change.has_value() && Config::has_changed(cfg_change)) {
    load_config();
  }

  if (next_brightness < max_brightness) {
    next_brightness++;
    next_color.setBrightness(next_brightness);
  }

  units.get<PinSpot>(unit_name::MAIN_SPOT)->colorNow(next_color);
  units.get<PinSpot>(unit_name::FILL_SPOT)->colorNow(next_color);

  if (next_brightness >= max_brightness) {
    next_color.rotateHue(hue_step);
  }

  set_finished(peaks.silence() == false);
}

void Standby::load_config() noexcept {
  static const auto base = toml::path{"fx.standby"};
  static const auto color_path = toml::path(base).append("color"sv);
  static const auto hue_path = toml::path(base).append("hue_step"sv);
  static const auto will_render_path = toml::path(base).append("will_render"sv);
  static const auto silence_path = toml::path(base).append("silence");

  should_render = Config().at(will_render_path).value_or(true);

  auto color_cfg = Config().table_at(color_path);

  next_color = Color({.hue = color_cfg["hue"sv].value_or(0.0),
                      .sat = color_cfg["sat"sv].value_or(0.0),
                      .bri = color_cfg["bri"sv].value_or(0.0)});

  hue_step = Config().at(hue_path).value_or(0.0);

  next_brightness = 0.0;
  max_brightness = next_color.brightness();

  auto silence_cfg = Config().table_at(silence_path);

  const auto silence_timeout_old = silence_timeout;
  silence_timeout = pet::from_val<Seconds, Minutes>(silence_cfg["timeout.seconds"].value_or(30));

  // start silence watch if timeout changed (at start or config reload)
  if (silence_timeout != silence_timeout_old) silence_watch();

  next_fx = silence_cfg["suggested_next_fx"].value_or(string("all_stop"));

  Config::want_changes(cfg_change);
}

void Standby::once() {
  load_config();

  units(unit_name::AC_POWER)->activate();
  units(unit_name::DISCO_BALL)->dark();

  units.get<ElWire>(unit_name::EL_DANCE)->dim();
  units.get<ElWire>(unit_name::EL_DANCE)->dim();
  units.get<LedForest>(unit_name::LED_FOREST)->dim();
}

void Standby::silence_watch() noexcept {

  silence_timer.expires_after(silence_timeout);
  silence_timer.async_wait([s = ptr()](const error_code ec) {
    if (!ec) {
      INFO(module_id, "SILENCE_WATCH", "timeout, setting finished\n");
      s->set_finished(true);
    }
  });
}

} // namespace fx
} // namespace pierre
