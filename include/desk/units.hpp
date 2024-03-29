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

#pragma once

#include "base/types.hpp"
#include "desk/dmx_data_msg.hpp"
#include "desk/unit.hpp"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <type_traits>

namespace pierre {

class Units {

private:
  static constexpr auto empty_excludes = std::initializer_list<string>();

public:
  Units() = default;

  void create_all_from_cfg() noexcept;

  void for_each(auto f, std::initializer_list<string> exclude_list = empty_excludes) noexcept {
    std::set<string> excludes(exclude_list);

    for (auto [name, unit] : map) {
      if (excludes.contains(name) == false) f(unit);
    }
  }

  void dark(std::initializer_list<string> exclude_list = empty_excludes) noexcept {
    for_each([](std::shared_ptr<Unit> unit) { unit->dark(); }, exclude_list);
  }

  bool empty() const noexcept { return map.empty(); }

  const auto operator()(const string &name) noexcept { return map.at(name); }

  template <typename T = Unit> constexpr auto get(const string &name) noexcept {
    auto unit = map[name];

    if constexpr (std::is_same_v<T, Unit>) {
      return unit;
    } else if constexpr (std::is_base_of_v<Unit, T>) {
      return static_pointer_cast<T>(unit);
    } else {
      static_assert(always_false_v<T>, "unhandled type");
    }
  }

  void prepare() noexcept {
    for_each([](std::shared_ptr<Unit> unit) { unit->prepare(); });
  }

  ssize_t ssize() const noexcept { return std::ssize(map); }

  void update_msg(DmxDataMsg &m) noexcept {
    for_each([&](std::shared_ptr<Unit> unit) mutable { unit->update_msg(m); });
  }

private:
  std::map<string, std::shared_ptr<Unit>> map;

public:
  static constexpr csv module_id{"desk::Units"};
};

} // namespace pierre