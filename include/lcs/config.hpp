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

class Config : public std::enable_shared_from_this<Config> {
private:
  Config(io_context &io_ctx, toml::table &cli_table) noexcept; // must be in .cpp to hide ArgsMap

public:
  static auto ptr() noexcept { return self->shared_from_this(); }
  static void init(io_context &io_ctx, toml::table &cli_table) noexcept;

  // raw, direct access
  template <typename P> const auto at(P p) noexcept {
    if constexpr (std::is_same_v<P, toml::path>) return live()[p];
    if constexpr (std::is_same_v<P, string_view> || std::is_same_v<P, string>) {
      const toml::path path{p};

      return live()[path];
    }
  }

  static bool ready() noexcept { return self && self->initialized.load(); }

  template <typename P> const auto table_at(P p) noexcept {
    if constexpr (std::is_same_v<P, toml::path>) return live()[p];
    if constexpr (std::is_same_v<P, string_view> || std::is_same_v<P, string>) {
      const toml::path path{p};

      return live()[path];
    }
  }

  // specific accessors
  static const string app_name() noexcept {
    static constexpr csv path{"app_name"};
    return self
        ->cli_table[path] //
        .value<string>()
        .value();
  }

  static bool daemon() noexcept {
    static constexpr csv path{"daemon"};
    return self->cli_table[path].value_or(false);
  }

  static const string build_time() noexcept {
    return self
        ->at(base_path("build_time"sv)) //
        .value_or(UNSET);
  };
  static const string build_vsn() noexcept {
    return self
        ->at(base_path("build_vsn"sv)) //
        .value_or(UNSET);
  }
  static const string config_vsn() noexcept {
    return self
        ->at(base_path("config_vsn"sv)) //
        .value_or(UNSET);
  }

  static fs::path fs_exec_path() noexcept {
    static constexpr csv path{"exec_path"};
    return fs::path(self->cli_table[path].ref<string>());
  }

  static fs::path fs_home() noexcept {
    static constexpr csv path{"home"};
    return fs::path(self->cli_table[path].ref<string>());
  }

  static fs::path fs_log_file() noexcept {
    static constexpr csv path{"log-file"};
    return fs::path(self->cli_table[path].ref<string>());
  }

  static fs::path fs_parent_path() noexcept {
    static constexpr csv path{"parent_path"};
    return fs::path(self->cli_table[path].ref<string>());
  }

  static fs::path fs_pid_path() noexcept {
    static constexpr csv path{"pid_file"};
    return fs::path(self->cli_table[path].ref<string>());
  }

  static bool has_changed(cfg_future &fut) noexcept {
    auto rc = false;

    if (fut.has_value() && fut->valid()) {
      if (auto status = fut->wait_for(0ms); status == std::future_status::ready) {
        rc = fut->get();
        fut.reset();
      }
    }

    return rc;
  }

  bool info_bool(csv mod, csv cat) noexcept {
    if (cat == csv{"info"}) return true;

    auto path = toml::path("info"sv);

    // order of precedence:
    //  1. info.<cat>       == boolean
    //  2. info.<mod>       == boolean
    //  3. info.<mod>.<cat> == boolean
    if (live()[toml::path(path).append(cat)].is_boolean()) {
      return live()[path.append(cat)].ref<bool>();
    } else if (live()[toml::path(path).append(mod)].is_boolean()) {
      return live()[path.append(mod)].ref<bool>();
    } else {
      return live()[path.append(mod).append(cat)].value_or(true);
    }
  }

  void monitor_file() noexcept;

  static const string receiver() noexcept; // see .cpp

  /// @brief Stop the file monitoring and reset the local self shared_ptr
  static void shutdown() noexcept {
    [[maybe_unused]] error_code ec;
    self->file_timer.cancel(ec);
  }

  static void want_changes(cfg_future &fut) noexcept;

private:
  // path builders
  static const toml::path base_path(csv key) noexcept {
    static constexpr csv p{"base"};
    return toml::path{p}.append(key);
  }

  const toml::table &live() noexcept {
    std::shared_lock slk(mtx, std::defer_lock);
    slk.lock();

    return tables.front();
  }

  bool parse(bool exit_on_error = false) noexcept;

private:
  // order dependent
  io_context &io_ctx;
  steady_timer file_timer;
  std::atomic_bool initialized;
  toml::table cli_table;

  // order independent
  fs::path full_path;
  cfg_future change_fut;
  fs::file_time_type last_write;
  std::list<toml::table> tables;
  std::optional<std::promise<bool>> change_proms;
  std::shared_mutex mtx;

public: // status messages
  string init_msg;
  string monitor_msg;
  string parse_msg;
  string banner_msg;

private:
  // class static data
  static constexpr auto EXIT_ON_FAILURE{true};
  static constexpr ccs UNSET{"?"};
  static constexpr csv BASE{"base"};
  static constexpr csv BUILD_TIME{"build_time"};
  static std::shared_ptr<Config> self;

public:
  static constexpr csv module_id{"config"};
};

// Config free functions

inline std::shared_ptr<Config> config() noexcept { return Config::ptr(); }
template <typename T> T config_val(csv path, T &&def_val) noexcept {
  return Config::ptr()->at(path).value_or<T>(std::forward<T>(def_val));
}

/// @brief Lookup the boolean value at info.<mod>.<cat>
/// @param mod module id
/// @param cat categpry
/// @return true or false
inline bool cfg_info(csv mod, csv cat) noexcept {
  return Config::ready() ? Config::ptr()->info_bool(mod, cat) : true;
}

} // namespace pierre