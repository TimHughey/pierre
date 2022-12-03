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

#include "desk/fx/leave.hpp"
#include "color.hpp"
#include "config/config.hpp"
#include "desk.hpp"
#include "unit/all.hpp"

namespace pierre {
namespace fx {

static cfg_future cfg_changed;

Leave::~Leave() { cfg_changed.reset(); }

void Leave::execute([[maybe_unused]] Peaks &peaks) {
  if (cfg_changed.has_value() && Config::has_changed(cfg_changed)) {
    load_config();
  }

  if (next_brightness < max_brightness) {
    next_brightness++;
    next_color.setBrightness(next_brightness);
  }

  units.get<PinSpot>(unit::MAIN_SPOT)->colorNow(next_color);
  units.get<PinSpot>(unit::FILL_SPOT)->colorNow(next_color);

  if (next_brightness >= max_brightness) {
    next_color.rotateHue(hue_step);
  }
}

void Leave::load_config() noexcept {
  static const auto base = toml::path{"fx.leave"};
  static const auto color_path = toml::path(base).append("color"sv);
  static const auto hue_path = toml::path(base).append("hue_step"sv);

  auto color_cfg = Config().table_at(color_path);

  next_color = Color({.hue = color_cfg["hue"sv].value_or(0.0),
                      .sat = color_cfg["sat"sv].value_or(0.0),
                      .bri = color_cfg["bri"sv].value_or(0.0)});

  hue_step = Config().at(hue_path).value_or(0.0);

  next_brightness = 0.0;
  max_brightness = next_color.brightness();

  Config::want_changes(cfg_changed);
}

void Leave::once() {
  load_config();

  units.get<AcPower>(unit::AC_POWER)->on();
  units.leave();
}

} // namespace fx
} // namespace pierre
