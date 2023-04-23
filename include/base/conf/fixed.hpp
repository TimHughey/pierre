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

#include <filesystem>

namespace pierre {
namespace conf {

struct fixed {

  using fs_path = std::filesystem::path;

  static fs_path app_data_dir() noexcept;
  static csv app_name() noexcept;
  static string cfg_dir() noexcept;
  static fs_path cfg_path() noexcept;
  static bool daemon() noexcept;
  static fs_path exec_dir() noexcept;
  static bool force_restart() noexcept;
  static string git() noexcept;
  static fs_path install_prefix() noexcept;
  static fs_path log_file() noexcept;
  static fs_path parent_dir() noexcept;
  static fs_path pid_file() noexcept;
};

} // namespace conf
} // namespace pierre