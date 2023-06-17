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

  const auto &base_table = tokc.table();

  base_table["units"].visit([this](const toml::array &arr) {
    arr.for_each([this](const toml::table &t) {
      t["type"].visit([&](const toml::value<string> &v) {
        string type = *v;
        std::unique_ptr<Unit> dev;

        if (type == "dimmable") {
          dev = std::make_unique<Dimmable>(t);
        } else if (type == "pinspot") {
          dev = std::make_unique<PinSpot>(t);
        } else if (type == "switch") {
          dev = std::make_unique<Switch>(t);
        }

        if ((bool)dev) {
          auto name = dev->name;
          auto [it, inserted] = map.insert_or_assign(std::move(name), std::move(dev));

          if (inserted) {
            INFO_AUTO("{}", *it->second);
          }
        }
      });
    });
  });
}

} // namespace desk
} // namespace pierre