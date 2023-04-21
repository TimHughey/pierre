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

namespace pierre {
namespace conf {

struct getv {

  static const string app_name() noexcept;
  static const string build_vsn() noexcept;
  static bool daemon() noexcept;
  static const string data_dir() noexcept;
  static bool force_restart() noexcept;
  static const string pid_file() noexcept;

  static const toml::table *mt() noexcept;
};

} // namespace conf
} // namespace pierre