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

#define TOML_IMPLEMENTATION // this translation unit compiles actual library
#include "lcs/config.hpp"
#include "base/elapsed.hpp"
#include "base/host.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "build_inject.hpp"
#include "git_version.hpp"
#include "lcs/args.hpp"
#include "lcs/logger.hpp"

#include <algorithm>
#include <any>
#include <cstdlib>
#include <filesystem>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/std.h>
#include <iostream>
#include <memory>

namespace pierre {

namespace fs = std::filesystem;

namespace shared {
std::unique_ptr<Config> config{nullptr};
}

// Config API
Config::Config(const toml::table &cli_table, asio::io_context &io_ctx) noexcept
    : cli_table(cli_table),                                           //
      _full_path(fs::path(build::info.sysconf_dir)                    //
                     .append(build::info.project)                     //
                     .append(cli_table["cfg-file"sv].ref<string>())), //
      file_watch(io_ctx, this)                                        //
{
  if (parse() == true) {
    init_msg = fmt::format("sizeof={:>5} table size={}", sizeof(Config), table.size());

    // if we made it here cli and config are parsed, toml tables are ready for use
  }
}

const string Config::app_name() noexcept {
  return shared::config->cli_table["app_name"sv].value<string>().value();
}

const string Config::banner_msg() noexcept {

  auto wrapped = [&table = shared::config->table]() -> const string {
    return string(table["project"].ref<string>())
        .append(" ")
        .append(table["git_describe"].ref<string>());
  };

  return wrapped();
}

bool Config::daemon() noexcept { return config()->cli_table["daemon"sv].value_or(false); }

fs::path Config::fs_data_path() noexcept { return shared::config->fs_path("data_dir"sv); }

fs::path Config::fs_path(csv path) noexcept {

  // create lamba for accessing shared::config
  auto wrapped = [s = shared::config.get()](csv p) -> fs::path {
    auto &t = s->table;
    const auto &app_name = s->cli_table["app_name"sv].ref<string>();

    // /use/local/share/pierre
    if (p == csv{"data_dir"}) {
      return fs::path(t[p].ref<string>()).append(app_name);
    }

    return fs::path(t["install_prefix"sv].ref<string>());
  };

  // invoke lambsa
  return wrapped(path);
}

bool Config::parse() noexcept {
  auto rc = false;

  parse_msg.clear();

  try {
    table = toml::parse_file(_full_path.c_str());

    table.emplace("project", build::info.project);
    table.emplace("git_describe", build::git.describe);
    table.emplace("install_prefix", build::info.install_prefix);
    table.emplace("sysconf_dir"sv, build::info.sysconf_dir);
    table.emplace("data_dir", build::info.data_dir);

    rc = true;

  } catch (const toml::parse_error &err) {
    parse_msg = fmt::format("{} parse failed: {}", _full_path, err.description());
  }

  return rc;
}

const string Config::receiver() noexcept { // static
  static constexpr csv fallback{"%h"};
  auto val = config()->at("mdns.receiver"sv).value_or(fallback);

  return (val == fallback ? string(Host().hostname()) : string(val));
}

} // namespace pierre