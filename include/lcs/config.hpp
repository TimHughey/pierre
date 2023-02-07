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
#include "io/io.hpp"
#include "lcs/types.hpp"

#define TOML_ENABLE_FORMATTERS 0 // don't need formatters
#define TOML_HEADER_ONLY 0       // reduces compile times
#include "toml++/toml.h"

#include <atomic>
#include <filesystem>
#include <future>
#include <list>
#include <optional>
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

class Config {
public:
  Config(io_context &io_ctx,
         const toml::table &cli_table) noexcept; // must be in .cpp to hide ArgsMap
  ~Config() noexcept;

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

  const string banner_msg() noexcept;

  static bool daemon() noexcept;

  static const string build_time() noexcept;
  static const string build_vsn() noexcept;

  static fs::path fs_exec_path() noexcept {
    static constexpr csv path{"exec_path"};
    return fs::path(shared::config->cli_table[path].ref<string>());
  }

  static fs::path fs_home() noexcept {
    static constexpr csv path{"home"};
    return fs::path(shared::config->cli_table[path].ref<string>());
  }

  static fs::path fs_log_file() noexcept {
    static constexpr csv path{"log-file"};
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

  static bool has_changed(cfg_future &fut) noexcept;

  void init() noexcept;

  bool log_bool(csv logger_module_id, csv mod, csv cat) noexcept;

  void monitor_file() noexcept;

  static bool ready() noexcept;

  static const string receiver() noexcept; // see .cpp

  static const string vsn() noexcept;

  static void want_changes(cfg_future &fut) noexcept;

private:
  const toml::table &live() noexcept;

  bool parse(bool exit_on_error = false) noexcept;

private:
  // order dependent
  io_context &io_ctx;
  toml::table cli_table;
  steady_timer file_timer;
  std::atomic_bool initialized;

  // order independent
  fs::path full_path;

  cfg_future change_fut;
  fs::file_time_type last_write;
  std::list<toml::table> tables;
  std::optional<std::promise<bool>> change_proms;
  std::shared_mutex mtx;
  std::shared_mutex want_changes_mtx;

public: // status messages
  string init_msg;
  string monitor_msg;
  string parse_msg;

private:
  // class static data
  static constexpr auto EXIT_ON_FAILURE{true};
  static constexpr ccs UNSET{"???"};

public:
  static constexpr csv module_id{"config"};
};

// Config free functions

inline auto config() noexcept { return shared::config.get(); }

template <typename T> toml::path config_path(csv key_path) noexcept {
  return toml::path(T::module_id).append(key_path);
}

template <typename T> T config_val(csv path, T &&def_val) noexcept {
  return shared::config->at(path).value_or<T>(std::forward<T>(def_val));
}

template <class C, typename T, typename D> inline auto config_val2(csv path, D &&def_val) noexcept {

  return shared::config->at(toml::path(C::module_id).append(path))
      .value_or<T>(std::forward<D>(def_val));
}

template <class C> int config_threads(int &&def_val) noexcept {
  return shared::config->at(toml::path(C::module_id).append("threads"))
      .value_or<int>(std::forward<int>(def_val));
}

/// @brief Lookup the boolean value at info.<mod>.<cat>
/// @param mod module id
/// @param cat categpry
/// @return true or false
inline bool cfg_logger(csv logger_mod, csv mod, csv cat) noexcept {
  return Config::ready() ? shared::config->log_bool(logger_mod, mod, cat) : true;
}

} // namespace pierre