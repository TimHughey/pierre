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

#include "base/conf/keys.hpp"
#include "base/conf/toml.hpp"
#include "base/types.hpp"

#include <filesystem>
#include <sstream>

namespace pierre {
namespace conf {

struct cli_args {

  friend struct fixed;

  cli_args(int argc, char **argv) noexcept;

  static bool error() noexcept { return !error_str.empty(); }
  static const string &error_msg() noexcept { return error_str; }

  static bool help() noexcept { return help_requested; }
  static auto &help_msg() noexcept { return help_ss; }

  static bool nominal_start() noexcept { return !help_requested && error_str.empty(); }

  static const auto &table() noexcept { return ttable; }

protected:
  static toml::table ttable;

private:
  static string error_str;
  static bool help_requested;
  static std::ostringstream help_ss;
  static string help_str;
};

} // namespace conf
} // namespace pierre