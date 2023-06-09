// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "units.hpp"
#include "base/conf/token.hpp"
#include "base/conf/toml.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"
#include "unit/all.hpp"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <type_traits>

namespace pierre {

namespace desk {

Units::Units() noexcept : tokc("desk"sv) {
  load_config();

  INFO_INIT("sizeof={:>5} unit_count={}", sizeof(Units), ssize());
}

void Units::load_config() noexcept {
  INFO_AUTO_CAT("load_config");

  if (tokc.empty()) {
    assert("missing desk (units) conf");
  }

  // gets a copy of the desk table
  auto table = tokc.table();

  // load dimmable units
  uint32_t max = tokc.val<uint32_t>("dimmable.max", 8190U);
  auto dimmable = table->at_path("dimmable.units").as_array();

  for (auto &&e : *dimmable) {

    const auto &t = *(e.as_table());

    string key = t["name"].value_or("unnamed");
    auto dev = std::make_unique<Dimmable>(t["name"].value_or("unnamed"), t["addr"].value_or(0UL));

    auto [it, inserted] = map.insert_or_assign(key, std::move(dev));

    auto apply_percent = [=](float percent) -> uint32_t { return max * percent; };

    if (inserted) {
      Dimmable *unit = static_cast<Dimmable *>(it->second.get());

      unit->config.max = apply_percent(t["max"].value_or(1.0));
      unit->config.min = apply_percent(t["min"].value_or(0.0));
      unit->config.dim = apply_percent(t["dim"].value_or(0.0));
      unit->config.bright = apply_percent(t["bright"].value_or(1.0));
      unit->config.pulse_start = apply_percent(t["pulse.start"].value_or(1.0));
      unit->config.pulse_end = apply_percent(t["pulse.end"].value_or(0.0));
    }
  }

  // load pinspot units
  INFO_AUTO("loading pinspots");

  auto pinspots = table->at_path("pinspot.units").as_array();
  for (auto &&e : *pinspots) {
    auto &t = *(e.as_table());

    string key = t["name"].value_or("unnamed");
    auto name = key;

    INFO_AUTO("key={} name={}", key, name);

    auto dev = std::make_unique<PinSpot>(std::move(name), t["addr"].value_or(0UL),
                                         t["frame_len"].value_or(0UL));
    map.insert_or_assign(key, std::move(dev));
  }

  // load switch units
  auto switches = table->at_path("switch.units").as_array();
  for (auto &&e : *switches) {
    const auto &t = *(e.as_table());

    string key = t["name"].value_or("unnamed");
    auto name = key;
    auto dev = std::make_unique<Switch>(std::move(name), t["addr"].value_or(0UL));
    map.insert_or_assign(key, std::move(dev));
  }
}

} // namespace desk
} // namespace pierre