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
#include "desk.hpp"
#include "lcs/config.hpp"
#include "lcs/config_watch.hpp"
#include "unit/all.hpp"

namespace pierre {
namespace fx {

Standby::~Standby() { cancel(); }

void Standby::execute(Peaks &peaks) noexcept {
  if (cfg_change.has_value() && cfg_watch_has_changed(cfg_change)) {
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

  // unless completed (silence timeout fired) set finished to false
  if (!completed()) {
    set_finished(peaks.silence() == false);
    next_fx = fx_name::MAJOR_PEAK;
  } else {
    next_fx = fx_name::ALL_STOP;
  }
}

void Standby::load_config() noexcept {

  static const auto base = toml::path{"fx.standby"};
  static const auto color_path = toml::path(base).append("color"sv);
  static const auto hue_path = toml::path(base).append("hue_step"sv);
  static const auto will_render_path = toml::path(base).append("will_render"sv);
  static const auto silence_timeout_path = toml::path(base).append("silence.timeout"sv);

  auto cfg = config();

  should_render = cfg->at(will_render_path).value_or(true);

  auto color_cfg = cfg->table_at(color_path);

  next_color = Color({.hue = color_cfg["hue"sv].value_or(0.0),
                      .sat = color_cfg["sat"sv].value_or(0.0),
                      .bri = color_cfg["bri"sv].value_or(0.0)});

  hue_step = cfg->at(hue_path).value_or(0.0);

  next_brightness = 0.0;
  max_brightness = next_color.brightness();

  auto timeout_cfg = cfg->table_at(silence_timeout_path);

  const auto silence_timeout_old = silence_timeout;
  silence_timeout = pet::from_val<Seconds, Minutes>(timeout_cfg["minutes"sv].value_or(30));

  // start silence watch if timeout changed (at start or config reload)
  if (silence_timeout != silence_timeout_old) silence_watch();

  cfg_watch_want_changes(cfg_change);
}

void Standby::once() {
  load_config();

  units(unit_name::AC_POWER)->activate();
  units(unit_name::DISCO_BALL)->dark();

  units.get<Dimmable>(unit_name::EL_DANCE)->dim();
  units.get<Dimmable>(unit_name::EL_DANCE)->dim();
  units.get<Dimmable>(unit_name::LED_FOREST)->dim();
}

void Standby::silence_watch() noexcept {

  silence_timer.expires_after(silence_timeout);
  silence_timer.async_wait([this](const error_code ec) {
    if (!ec) {
      next_fx = fx_name::ALL_STOP;
      set_finished(true);
    }
  });
}

} // namespace fx
} // namespace pierre
