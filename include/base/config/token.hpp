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

class token {
private:
  using tokens_t = std::vector<std::reference_wrapper<token>>;

public:
  using lambda = std::function<void(std::any &&next_table)>;

public:
  /// @brief Create config token with populated subtable that is not registered
  ///        for change notifications
  /// @param mid module_id (aka key)
  token(csv mid) noexcept;

  /// @brief Create config token with populated subtable and initializes file watch
  /// @param mid
  /// @param io_ctx
  token(csv mid, asio::io_context &io_ctx, std::any &&cli_table) noexcept;
  ~token() noexcept;

  token(token &&) = default;
  token &operator=(token &&) = default;

  const string build_vsn() noexcept;

  bool daemon() noexcept;

  const string data_path() noexcept;

  template <typename T> const T get() const noexcept { return std::any_cast<T>(table); }

  template <typename T> static inline const T get(auto &any_table) noexcept {
    return std::any_cast<T>(any_table);
  }

  static const string &get_parse_msg() noexcept { return parse_msg; }
  static const string &get_init_msg() noexcept { return init_msg; }

  void notify_via(auto &executor) noexcept {
    handler = [&, this](std::any &&next_table) mutable {
      asio::post(executor,                                                    //
                 asio::append([this](std::any nt) { table = std::move(nt); }, //
                              std::move(next_table)));
    };
  }

  static bool parse_ok() noexcept { return parse_msg.empty(); }
  static const string &parse_error() noexcept { return parse_msg; }

  template <typename ReturnType, typename T>
  inline ReturnType val(auto path, const auto &&def_val) noexcept {

    const auto &t = std::any_cast<T>(table);

    if constexpr (IsDuration<ReturnType>) {
      const auto v = t[path].value_or(def_val);

      return ReturnType(v);

    } else if constexpr (std::same_as<ReturnType, decltype(def_val)>) {
      return t[path].value_or(std::forward<decltype(def_val)>(def_val));

    } else if constexpr (std::constructible_from<ReturnType, decltype(def_val)>) {
      return t[path].value_or(ReturnType{def_val});

    } else {
      return t[path].value_or(std::forward<ReturnType>(def_val));
    }
  }

  void set_custom_handler(auto &&h) noexcept { handler = std::move(h); }

  void update_table(std::any &&t) noexcept { table = std::move(t); }

private:
  void check_file() noexcept;
  static std::any copy_from_master(csv mid) noexcept;

  void emplace_token() noexcept;

  void monitor_file() noexcept;

  void notify_of_change(std::any &&next_table) noexcept;

  bool parse() noexcept;

private:
  // order dependent
  string mod_id;
  lambda handler = [](std::any &&) {};
  std::any table;

private:
  static std::any cli_tbl;
  static std::any master_tbl;
  static tokens_t tokens;
  static std::mutex tokens_mtx;

  static string init_msg;
  static string monitor_msg;
  static string parse_msg;

public:
  static constexpr csv module_id{"config.token"};
};

} // namespace conf
} // namespace pierre