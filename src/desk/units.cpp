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
#include "base/logger.hpp"
#include "base/types.hpp"
#include "config/config.hpp"
#include "unit/all.hpp"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <type_traits>

namespace pierre {

void Units::create_all_from_cfg() noexcept {
  auto cfg_desk = config()->at("desk"sv);

  {
    // load dimmable units
    auto cfg = cfg_desk["dimmable"sv];
    uint32_t max = cfg["max"sv].value_or(8190);

    for (auto &&e : *cfg["units"sv].as_array()) {
      auto t = e.as_table();

      const auto name = (*t)["name"sv].value_or("unnamed");
      const size_t addr = (*t)["addr"sv].value_or(0UL);

      const hdopts opts{.name = name, .type = unit_type::DIMMABLE, .address = addr};

      auto [it, inserted] = map.try_emplace(name, std::make_shared<Dimmable>(std::move(opts)));

      auto apply_percent = [=](float percent) -> uint32_t { return max * percent; };

      if (inserted) {
        auto unit = static_pointer_cast<Dimmable>(it->second);

        unit->config.max = apply_percent((*t)["max"sv].value_or(1.0));
        unit->config.min = apply_percent((*t)["min"sv].value_or(0.0));
        unit->config.dim = apply_percent((*t)["dim"sv].value_or(0.0));
        unit->config.bright = apply_percent((*t)["bright"sv].value_or(1.0));
        unit->config.pulse_start = apply_percent((*t)["pulse.start"sv].value_or(1.0));
        unit->config.pulse_end = apply_percent((*t)["pulse.end"sv].value_or(0.0));
      }
    }
  }

  { // load pinspot units
    auto cfg = cfg_desk["pinspot"sv];
    for (auto &&e : *cfg["units"sv].as_array()) {
      auto t = e.as_table();

      const auto name = (*t)["name"sv].value_or("unnamed");
      const size_t addr = (*t)["addr"sv].value_or(0UL);
      const size_t frame_len = (*t)["frame_len"sv].value_or(0UL);

      const hdopts opts{.name = name, .type = unit_type::PINSPOT, .address = addr};

      map.try_emplace(name, std::make_shared<PinSpot>(std::move(opts), frame_len));
    }
  }

  { // load pinspot units
    auto cfg = cfg_desk["switch"sv];
    for (auto &&e : *cfg["units"sv].as_array()) {
      auto t = e.as_table();

      const auto name = (*t)["name"sv].value_or("unnamed");
      const size_t addr = (*t)["addr"sv].value_or(0UL);

      const hdopts opts{.name = name, .type = unit_type::SWITCH, .address = addr};

      map.try_emplace(name, std::make_shared<Switch>(std::move(opts)));
    }
  }
}

} // namespace pierre