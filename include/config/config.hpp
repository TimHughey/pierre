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

#include "base/io.hpp"
#include "base/types.hpp"
#include "config/types.hpp"

#include <filesystem>
#include <future>
#include <list>
#include <optional>
#include <shared_mutex>
#include <type_traits>

#define TOML_ENABLE_FORMATTERS 0 // don't need formatters
#define TOML_HEADER_ONLY 0       // reduces compile times
#include <toml++/toml.h>

namespace pierre {

namespace {
namespace fs = std::filesystem;
}

class Config : public std::enable_shared_from_this<Config> {
private:
  Config(io_context &io_ctx, int argc, char **argv) noexcept; // must be in .cpp to hide ArgsMap

public:
  static auto init(io_context &io_ctx, int argc, char *argv[]) noexcept {
    if (self.use_count() < 1) {
      self = std::shared_ptr<Config>(new Config(io_ctx, argc, argv));
    }

    return ptr();
  }

  // raw, direct access
  template <typename P> const auto at(P p) noexcept {
    std::shared_lock slk(mtx, std::defer_lock);
    slk.lock();

    if constexpr (std::is_same_v<P, toml::path>) return table()[p];
    if constexpr (std::is_same_v<P, string_view> || std::is_same_v<P, string>) {
      const toml::path path{p};

      return table()[path];
    }
  }

  static std::shared_ptr<Config> ptr() noexcept { return self->shared_from_this(); }

  static bool ready() noexcept { return self->initialized; }

  const toml::table &table() noexcept {
    std::shared_lock slk(mtx, std::defer_lock);
    slk.lock();

    return tables.front();
  }

  template <typename P> const auto table_at(P p) noexcept {
    std::shared_lock slk(mtx, std::defer_lock);
    slk.lock();

    if constexpr (std::is_same_v<P, toml::path>) return table()[p];
    if constexpr (std::is_same_v<P, string_view> || std::is_same_v<P, string>) {
      const toml::path path{p};

      return table()[path];
    }
  }

  // specific accessors
  const string app_name() noexcept { return at(cli("app_name"sv)).value_or(UNSET); }
  const string build_time() noexcept { return at(base(BUILD_TIME)).value_or(UNSET); };
  const string build_vsn() noexcept { return at("base.build_vsn"sv).value_or(UNSET); };
  const string config_vsn() noexcept { return at("base.config_vsn"sv).value_or(UNSET); }
  fs::path fs_exec_path() noexcept {
    return fs::path(at(cli("exec_path"sv)).value_or(string("/")));
  }

  fs::path fs_parent_path() noexcept {
    return fs::path(at(cli("parent_path"sv)).value_or(string("/")));
  }

  static bool has_changed(cfg_future &fut) noexcept;

  void monitor_file() noexcept;

  const string receiver() noexcept; // see .cpp, uses Host

  static bool should_start() noexcept { return self->will_start; }

  static void want_changes(cfg_future &fut) noexcept;
  const string working_dir() noexcept { return table_at("base.working_dir"sv).value_or(UNSET); }

private:
  // path builders
  const toml::path cli(auto key) noexcept { return toml::path(CLI).append(key); }
  const toml::path base(csv key) noexcept { return toml::path{"base"sv}.append(key); }

  bool parse() noexcept;

private:
  // order dependent
  io_context &io_ctx;
  steady_timer file_timer;
  bool initialized;
  bool will_start;
  const string home_dir;

  // order independent
  fs::path full_path;
  cfg_future change_fut;
  fs::file_time_type last_write;
  std::list<toml::table> tables;
  std::optional<std::promise<bool>> change_proms;
  std::shared_mutex mtx;

  // class static data
  static std::shared_ptr<Config> self;
  static constexpr csv BASE{"base"};
  static constexpr csv BUILD_TIME{"build_time"};
  static constexpr csv CLI{"cli"};
  static constexpr ccs UNSET{"?"};

public:
  static constexpr csv module_id{"CONFIG"};
};

inline std::shared_ptr<Config> config() noexcept { return Config::ptr(); }

} // namespace pierre