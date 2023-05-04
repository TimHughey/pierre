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

#include <array>
#include <concepts>
#include <fmt/format.h>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <type_traits>

namespace pierre {
namespace conf {

class watch;

class token {
  friend struct fmt::formatter<token>;
  friend class watch;

public:
  /// @brief Create a default token (does not point to a configuration)
  token() = default;

  /// @brief Create config token with populated subtable that is not registered
  ///        for change notifications
  /// @param mid module_id (aka root)
  token(csv mid) noexcept;
  token(csv mid, watch *watcher) noexcept;

  ~token() noexcept;

  token(token &&) = default;

public:
  static token &acquire_watch_token(csv mid) noexcept;
  token &operator=(token &&) = default;

  bool changed() noexcept { return std::exchange(has_changed, false); }

  bool empty() const noexcept { return ttable.empty(); }

  bool latest() noexcept;

  void initiate() noexcept {}
  void initiate_watch() noexcept;

  bool is_table() const noexcept { return ttable.is_table(); }

  const string &msg(ParseMsg id) const noexcept { return msgs[id]; }

  template <typename ReturnType> inline ReturnType val(auto &&p, const auto &&def_val) noexcept {
    const auto path = root + p;

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

  bool parse_ok() const noexcept { return msgs[Parser].empty(); }

  toml::table *table() noexcept { return ttable[root].as<toml::table>(); }

protected:
  void add_msg(ParseMsg msg_id, const string m) noexcept { msgs[msg_id] = m; }
  bool changed(bool b) noexcept { return std::exchange(has_changed, b); }

protected:
  string uuid;
  toml::path root;
  toml::table ttable;
  parse_msgs_t msgs;

  bool has_changed{false};
  watch *watcher{nullptr};

public:
  static constexpr csv module_id{"conf.token"};
};

} // namespace conf
} // namespace pierre

template <> struct fmt::formatter<pierre::conf::token> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::conf::token &tok, FormatContext &ctx) const {

    return formatter<std::string>::format(
        fmt::format("{} {} size={}", tok.root.str(), tok.uuid, tok.ttable.size()), ctx);
  }
};