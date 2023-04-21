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

void Units::create_all_from_cfg() noexcept {
  // gets a copy of the desk table
  conf::token ctoken("desk");
  const auto table = ctoken.conf_table();

  {
    // load dimmable units

    uint32_t max = ctoken.conf_val<uint32_t>("max"_tpath, 8190U);
    auto units = table->at_path("dimmable.units").as_array();

    for (auto &&e : *units) {

      const auto &t = *(e.as_table());

      const auto name = t["name"_tpath].value_or("unnamed");
      const size_t addr = t["addr"_tpath].value_or(0UL);

      const hdopts opts{.name = name, .type = unit_type::DIMMABLE, .address = addr};

      auto [it, inserted] = map.try_emplace(name, std::make_unique<Dimmable>(std::move(opts)));

      auto apply_percent = [=](float percent) -> uint32_t { return max * percent; };

      if (inserted) {
        Dimmable *unit = static_cast<Dimmable *>(it->second.get());

        unit->config.max = apply_percent(t["max"_tpath].value_or(1.0));
        unit->config.min = apply_percent(t["min"_tpath].value_or(0.0));
        unit->config.dim = apply_percent(t["dim"_tpath].value_or(0.0));
        unit->config.bright = apply_percent(t["bright"_tpath].value_or(1.0));
        unit->config.pulse_start = apply_percent(t["pulse.start"_tpath].value_or(1.0));
        unit->config.pulse_end = apply_percent(t["pulse.end"_tpath].value_or(0.0));
      }
    }
  }

  { // load pinspot units
    auto units = table->at_path("pinspot.units"_tpath).as_array();
    for (auto &&e : *units) {
      auto &t = *(e.as_table());

      const auto name = t["name"_tpath].value_or("unnamed");
      const size_t addr = t["addr"_tpath].value_or(0UL);
      const size_t frame_len = t["frame_len"_tpath].value_or(0UL);

      const hdopts opts{.name = name, .type = unit_type::PINSPOT, .address = addr};

      map.try_emplace(name, std::make_unique<PinSpot>(std::move(opts), frame_len));
    }
  }

  { // load pinspot units
    auto units = table->at_path("switch.units"_tpath).as_array();
    for (auto &&e : *units) {
      const auto &t = *(e.as_table());

      const auto name = t["name"_tpath].value_or("unnamed");
      const size_t addr = t["addr"_tpath].value_or(0UL);

      const hdopts opts{.name = name, .type = unit_type::SWITCH, .address = addr};

      map.try_emplace(name, std::make_unique<Switch>(std::move(opts)));
    }
  }
}

} // namespace desk
} // namespace pierre