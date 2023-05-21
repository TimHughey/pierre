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

#include "base/conf/token.hpp"
#include "base/types.hpp"
#include "desk/msg/data.hpp"
#include "desk/unit.hpp"

#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <type_traits>

namespace pierre {
namespace desk {

class Units {

private:
  static constexpr auto empty_excludes = std::initializer_list<string>();

public:
  Units() noexcept;

  void for_each(auto f, std::initializer_list<string> exclude_list = empty_excludes) noexcept {
    std::set<string> excludes(exclude_list);

    for (auto &[name, unit] : map) {
      if (excludes.contains(name) == false) f(unit.get());
    }
  }

  void dark(std::initializer_list<string> exclude_list = empty_excludes) noexcept {
    for_each([](Unit *unit) { unit->dark(); }, exclude_list);
  }

  bool empty() const noexcept { return map.empty(); }

  const auto *operator()(const string &name) noexcept { return map.at(name).get(); }

  template <typename T> inline auto get(const auto name) noexcept {
    auto unit = map[name].get();

    if constexpr (std::same_as<T, Unit>) {
      return unit;
    } else if constexpr (std::derived_from<T, Unit>) {
      return static_cast<T *>(unit);
    } else {
      static_assert(AlwaysFalse<T>, "unhandled type");
    }
  }

  void prepare() noexcept {
    for_each([](Unit *unit) { unit->prepare(); });
  }

  ssize_t ssize() const noexcept { return std::ssize(map); }

  void update_msg(DataMsg &m) noexcept {
    for_each([&](Unit *unit) mutable { unit->update_msg(m); });
  }

private:
  void load_config() noexcept;

private:
  // order dependent
  conf::token tokc;

  // order independent
  std::map<string, std::unique_ptr<Unit>> map;

public:
  static constexpr auto module_id{"desk.units"sv};
};

} // namespace desk
} // namespace pierre