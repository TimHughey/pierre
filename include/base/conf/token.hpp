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
#include "base/conf/toml.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"

#include <concepts>
#include <fmt/format.h>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>

namespace pierre {
namespace conf {

class token {

public:
  /// @brief Create config token with populated subtable that is not registered
  ///        for change notifications
  /// @param mid module_id (aka key)
  token(csv mid) noexcept;
  virtual ~token() noexcept {}

  token(token &&) = default;
  token &operator=(token &&) = default;

  friend struct fmt::formatter<token>;

  toml::table *conf_table() noexcept { return ttable.as<toml::table>(); }

  template <typename ReturnType>
  inline ReturnType conf_val(auto &&p, const auto &&def_val) noexcept {
    toml::path path(p);

    if constexpr (IsDuration<ReturnType>) {
      return ReturnType(ttable[path].value_or(def_val));
    } else if constexpr (std::same_as<ReturnType, decltype(def_val)>) {
      return ttable[path].value_or(std::forward<decltype(def_val)>(def_val));
    } else if constexpr (std::constructible_from<ReturnType, decltype(def_val)>) {
      return ttable[path].value_or(ReturnType{def_val});
    } else {
      return ttable[path].value_or(std::forward<ReturnType>(def_val));
    }
  }

private:
  // order dependent
  const toml::path root;

  // order independent
  toml::table ttable;

public:
  static constexpr csv module_id{"conf.token"};
};

} // namespace conf
} // namespace pierre

template <> struct fmt::formatter<pierre::conf::token> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(pierre::conf::token &token, FormatContext &ctx) const {

    return formatter<std::string>::format(
        fmt::format("root={} table_size={}", token.root.str(), token.ttable.size()), ctx);
  }
};