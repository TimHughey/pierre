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

#include "base/asio.hpp"
#include "base/conf/keys.hpp"

#include "base/conf/toml.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"

#include <boost/asio/append.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>

namespace pierre {
namespace conf {

class master;
extern std::shared_ptr<master> mptr;

class master : std::enable_shared_from_this<master> {
public:
  // note: ordered by relevance
  enum MSG_TYPE : uint8_t { HelpMsg = 0, ArgsErrMsg, ParseMsg, InitMsg };

public:
  master(int argc, char *argv[]) noexcept;
  ~master() = default;

  friend struct getv;

  void copy_to(const toml::path &root, toml::table &dest) {
    if (ttable[root].is_table()) {
      dest = ttable[root].ref<toml::table>();
    } else {
      dest = toml::table();
    }
  }

  static auto create(int argc, char *argv[]) noexcept {
    mptr = std::make_shared<master>(argc, argv);

    return mptr;
  }

  const string &get_first_msg() const noexcept;
  const string &get_msg(MSG_TYPE t) const noexcept { return msgs[t]; }

  bool nominal_start() const noexcept;

  bool parse_ok() noexcept { return msgs[ParseMsg].empty(); }
  const string &parse_error() noexcept { return msgs[ParseMsg]; }

  template <typename ReturnType, typename T>
  static inline ReturnType val(auto raw_path, const auto &&def_val) noexcept {
    const auto p = mptr.get();

    toml::path path(make_path(raw_path));

    if constexpr (IsDuration<ReturnType>) {
      return ReturnType(ttable[path].value_or(def_val));

    } else if constexpr (std::same_as<ReturnType, decltype(def_val)>) {
      return ttable[path].value_or(std::forward<decltype(def_val)>(def_val));

    } else if constexpr (std::constructible_from<ReturnType, decltype(def_val)>) {
      return ttable[path].value_or(ReturnType{def_val});
    }

    return ttable[path].value_or(std::forward<ReturnType>(def_val));
  }

  const auto &table_direct() const noexcept { return ttable; }

  // void set_custom_handler(auto &&h) noexcept { handler = std::move(h); }

  // void update_table(std::any &&t) noexcept { table = std::move(t); }

private:
  // void check_file() noexcept;
  // static std::any copy_from_master(csv mid) noexcept;

  // void emplace_token() noexcept;

  static toml::path make_path(auto raw) noexcept {

    if constexpr (std::same_as<decltype(raw), toml::path>) {
      return raw;
    } else if constexpr (std::constructible_from<toml::path, decltype(raw)>) {
      return toml::path(raw);
    } else {
      static_assert(always_false_v<decltype(raw)>, "unable to convert to toml::path");
    }
  }

  static toml::path make_path_build(auto raw) noexcept {
    return toml::path(root::build).append(raw);
  }

  static toml::path make_path_cli(auto raw) noexcept { return toml::path(root::cli).append(raw); }

  // void monitor_file() noexcept;

  // void notify_of_change(std::any &&next_table) noexcept;

  bool parse() noexcept;

  toml::table *table() noexcept { return ttable.as<toml::table>(); }

protected:
  // order dependent
  static toml::table ttable;

  // order independent
  std::array<string, 4> msgs;

public:
  static constexpr csv module_id{"config.master"};
};

} // namespace conf
} // namespace pierre