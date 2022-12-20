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

#include "base/threads.hpp"
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

class Config {
public:
  Config() = default;

  // initialization and setup
  static Config init(int argc, char **argv) noexcept;
  bool ready() const noexcept { return initialized; }

  // raw, direct access
  template <typename P> const auto at(P p) const noexcept {
    std::shared_lock slk(mtx, std::defer_lock);
    slk.lock();

    if constexpr (std::is_same_v<P, toml::path>) return table()[p];
    if constexpr (std::is_same_v<P, string_view> || std::is_same_v<P, string>) {
      const toml::path path{p};

      return table()[path];
    }
  }

  const toml::table &table() const noexcept {
    std::shared_lock slk(mtx, std::defer_lock);
    slk.lock();

    return tables.front();
  }

  template <typename P> const auto table_at(P p) const noexcept {
    std::shared_lock slk(mtx, std::defer_lock);
    slk.lock();

    if constexpr (std::is_same_v<P, toml::path>) return table()[p];
    if constexpr (std::is_same_v<P, string_view> || std::is_same_v<P, string>) {
      const toml::path path{p};

      return table()[path];
    }
  }

  // specific accessors
  const string app_name() const noexcept { return at(cli("app_name"sv)).value_or(UNSET); }
  const string build_time() const noexcept { return at(base(BUILD_TIME)).value_or(UNSET); };
  const string build_vsn() const noexcept { return at("base.build_vsn"sv).value_or(UNSET); };
  const string config_vsn() const noexcept { return at("base.config_vsn"sv).value_or(UNSET); }
  fs::path fs_exec_path() const noexcept {
    return fs::path(at(cli("exec_path"sv)).value_or(string("/")));
  }

  fs::path fs_parent_path() const noexcept {
    return fs::path(at(cli("parent_path"sv)).value_or(string("/")));
  }

  static bool has_changed(cfg_future &fut) noexcept;

  const string receiver() const noexcept; // see .cpp, uses Host

  static bool should_start() noexcept { return will_start; }

  static void want_changes(cfg_future &fut) noexcept;
  const string working_dir() noexcept { return table_at("base.working_dir"sv).value_or(UNSET); }

private:
  // path builders
  const toml::path cli(auto key) const noexcept { return toml::path(CLI).append(key); }
  const toml::path base(csv key) const noexcept { return toml::path{"base"sv}.append(key); }

  static bool parse() noexcept;
  static void monitor_file() noexcept;

private:
  static bool initialized;
  static bool will_start;
  static cfg_future change_fut;
  static constexpr int CONFIG_THREADS{1};
  static fs::path full_path;
  static std::filesystem::file_time_type last_write;
  static std::list<toml::table> tables;
  static std::optional<std::promise<bool>> change_proms;
  static std::shared_mutex mtx;
  static Threads threads;

  static constexpr csv BASE{"base"};
  static constexpr csv BUILD_TIME{"build_time"};
  static constexpr csv CLI{"cli"};
  static constexpr ccs UNSET{"?"};

public:
  static constexpr csv TASK_NAME{"Config"};
  static constexpr csv module_id{"CONFIG"};
};

} // namespace pierre