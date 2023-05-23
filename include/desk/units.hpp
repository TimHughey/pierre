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
#include <initializer_list>
#include <map>
#include <memory>
#include <set>

namespace pierre {
namespace desk {

/// @brief Rendering Unit keeper and helper
class Units {
  static constexpr std::initializer_list<string> empty_excludes{};

  /// @brief Helper function for executing a lambda on units
  /// @param f lambda, takes a single raw Unit pointer
  /// @param exclude_list Unit names to skip
  void for_each(auto f, std::set<string> excludes = empty_excludes) noexcept {
    for (auto &[name, unit] : map) {
      if (excludes.contains(name) == false) f(unit.get());
    }
  }

public:
  Units() noexcept;

  /// @brief Set all Units, minus exclude list, to dark
  /// @param excludes
  void dark(std::set<string> excludes = empty_excludes) noexcept {
    for_each([](Unit *unit) { unit->dark(); }, std::move(excludes));
  }

  /// @brief Is the container of Units empty (uninitialized)
  /// @return boolean
  bool empty() const noexcept { return map.empty(); }

  /// @brief Functor to return a raw Unit (base class) pointer
  /// @param name Name of desired Unit
  /// @return Raw pointer to Unit (base class)
  const auto *operator()(const string &name) noexcept { return map.at(name).get(); }

  /// @brief Execute all Units prepare hook
  void prepare() noexcept {
    for_each([](Unit *unit) { unit->prepare(); });
  }

  /// @brief Acquire a raw pointer to a specific Unit subclass
  /// @tparam T Desired Unit subclass
  /// @param name Desired unit name
  /// @return Raw pointer to the Unit subclass
  template <typename T> auto ptr(const auto name) noexcept {
    auto unit = map[name].get();

    if constexpr (std::same_as<T, Unit>) {
      return unit;
    } else if constexpr (std::derived_from<T, Unit>) {
      return static_cast<T *>(unit);
    } else {
      static_assert(AlwaysFalse<T>, "unhandled type");
    }
  }

  /// @brief Signed size of Units container
  /// @return ssize_t
  ssize_t ssize() const noexcept { return std::ssize(map); }

  /// @brief Execute all Units update msg hook
  /// @param m DataMsg to pass to all units
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
  MOD_ID("desk.units");
};

} // namespace desk
} // namespace pierre