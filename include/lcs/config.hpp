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

#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "lcs/types.hpp"

#define TOML_ENABLE_FORMATTERS 0 // don't need formatters
#define TOML_HEADER_ONLY 0       // reduces compile times
#include "toml++/toml.h"

#include <atomic>
#include <concepts>
#include <filesystem>
#include <list>
#include <shared_mutex>
#include <type_traits>

namespace pierre {

namespace {
namespace fs = std::filesystem;
}

class Config;

namespace shared {
extern std::unique_ptr<Config> config;
}

struct build_info_t {
  const string vsn;
  const string install_prefix;
  const toml::table &cli_table;
};

class Config {
public:
  Config(const toml::table &cli_table) noexcept;
  ~Config() = default;

  // raw, direct access
  template <typename P> const auto at(P p) noexcept {
    if constexpr (std::is_same_v<P, toml::path>) return live()[p];
    if constexpr (std::is_same_v<P, string_view> || std::is_same_v<P, string>) {
      const toml::path path{p};

      return live()[path];
    }
  }

  template <typename P> const auto table_at(P p) noexcept {
    if constexpr (std::is_same_v<P, toml::path>) return live()[p];
    if constexpr (std::is_same_v<P, string_view> || std::is_same_v<P, string>) {
      const toml::path path{p};

      return live()[path];
    }
  }

  // specific accessors
  static const string app_name() noexcept;

  static const string banner_msg() noexcept;

  static bool daemon() noexcept;

  static fs::path fs_data_path() noexcept;

  static fs::path fs_exec_path() noexcept {
    static constexpr csv path{"exec_path"};
    return fs::path(shared::config->cli_table[path].ref<string>());
  }

  static fs::path fs_log_file() noexcept {
    static constexpr csv path{"log_file"};
    return fs::path(shared::config->cli_table[path].ref<string>());
  }

  static fs::path fs_parent_path() noexcept {
    static constexpr csv path{"parent_path"};
    return fs::path(shared::config->cli_table[path].ref<string>());
  }

  static fs::path fs_pid_path() noexcept {
    static constexpr csv path{"pid_file"};
    return fs::path(shared::config->cli_table[path].ref<string>());
  }

  const toml::table &live() noexcept;

  bool log_bool(csv logger_module_id, csv mod, csv cat) noexcept;

  static bool ready() noexcept;

  static const string receiver() noexcept; // see .cpp

protected:
  friend class ConfigWatch;

  static fs::path fs_path(csv path) noexcept; // return non-const to allow append
  const auto file_path() const noexcept { return _full_path; }
  bool parse() noexcept;

private:
  // order dependent
  toml::table cli_table;
  const fs::path _full_path;
  std::atomic_bool initialized;

  // order independent
  std::list<toml::table> tables;
  std::shared_mutex mtx;

public: // status messages
  string init_msg;
  string parse_msg;

public:
  static constexpr csv module_id{"config"};
  static constexpr ccs UNSET{"???"};
};

////
//// Config free functions
////

// must be first, others need it
inline auto config() noexcept { return shared::config.get(); }

inline const auto app_name() noexcept {
  return shared::config->at("app_name"sv).value_or<string>(Config::UNSET);
}

inline const auto cfg_build_vsn() noexcept {
  return shared::config->at("git_describe"sv).value_or<string>(Config::UNSET);
}

inline const toml::table cfg_copy_live() noexcept { return toml::table(config()->live()); }

/// @brief Lookup the boolean value at info.<mod>.<cat>
/// @param mod module id
/// @param cat categpry
/// @return true or false
inline bool cfg_logger(csv logger_mod, csv mod, csv cat) noexcept {
  return Config::ready() ? shared::config->log_bool(logger_mod, mod, cat) : true;
}

template <typename T> toml::path config_path(csv key_path) noexcept {
  return toml::path(T::module_id).append(key_path);
}

template <class Caller, typename DefaultVal>
inline auto config_val(const toml::table &t, csv path, DefaultVal &&def_val) noexcept {
  using ReturnType = decltype(def_val);
  const auto full_path = toml::path(Caller::module_id).append(path);

  if constexpr (IsDuration<ReturnType> && std::integral<DefaultVal>) {
    return ReturnType(t[full_path].value_or(std::forward<DefaultVal>(def_val)));
  } else {
    return t[full_path].value_or<ReturnType>(std::forward<DefaultVal>(def_val));
  }
}

template <class Caller, typename ReturnType, typename DefaultVal>
inline auto config_val(csv path, DefaultVal &&def_val) noexcept {

  if constexpr (IsDuration<ReturnType> && std::integral<DefaultVal>) {
    const auto raw = shared::config->at(toml::path(Caller::module_id).append(path))
                         .value_or(std::forward<DefaultVal>(def_val));

    return ReturnType(raw);
  } else {
    return shared::config->at(toml::path(Caller::module_id).append(path))
        .value_or<ReturnType>(std::forward<DefaultVal>(def_val));
  }
}

template <class C> int config_threads(int &&def_val) noexcept {
  return shared::config->at(toml::path(C::module_id).append("threads"))
      .value_or<int>(std::forward<int>(def_val));
}

} // namespace pierre