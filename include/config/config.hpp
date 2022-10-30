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

#include <optional>

#define TOML_ENABLE_FORMATTERS 0 // don't need formatters
#define TOML_HEADER_ONLY 0       // reduces compile times
#include <toml++/toml.h>

namespace pierre {

class Config {
public:
  Config() = default;

  // initialization and setup
  static Config init(int argc, char **argv) noexcept {
    auto cfg = Config();
    cfg.init_self(argc, argv);

    return cfg;
  }

  bool ready() const noexcept { return initialized; }

  // raw, direct access
  toml::table *table_at(csv path);
  toml::table &table() { return _table; }

  // specific accessors
  const string app_name() const noexcept {
    return _table.at_path("cli.app_name"sv).value_or(UNSET);
  }

  const string build_time() const noexcept {
    return _table.at_path("base.build_time"sv).value_or(UNSET);
  };

  const string build_vsn() const noexcept {
    return _table.at_path("base.build_vsn"sv).value_or(UNSET);
  };

  const string config_vsn() const noexcept {
    return _table.at_path("base.config_vsn"sv).value_or(UNSET);
  }

  const string receiver() const noexcept; // see .cpp, uses Host

  const string working_dir() const noexcept {
    return _table.at_path("base.working_dir"sv).value_or(UNSET);
  }

private:
  void init_self(int argc, char **argv) noexcept;

private:
  static toml::table _table;
  static bool initialized;

  static constexpr ccs UNSET{"?"};

public:
  static constexpr csv module_id{"CONFIG2"};
};

} // namespace pierre