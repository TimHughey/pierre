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

/// @brief Helper class for access to global cli args, build and
///        runtime configuration.
///
///        **NOTE** Information only available after creation of cli_args static class
struct fixed {

  using fs_path = std::filesystem::path;

  /// @brief Appliation data directory (e.g. /usr/share/pierre)
  /// @return modifiable copy of std::filesystem::path
  static fs_path app_data_dir() noexcept;

  /// @brief Application name from CMakeList project definition (e.g. pierre)
  /// @return constant string view
  static csv app_name() noexcept;

  /// @brief Full path and filename of the configuration file as determined
  ///        based on cli args (or default).
  /// @return modifiable string copy
  ///         string is returned based on downstream usage
  static string cfg_file() noexcept;

  /// @brief Should this invocatio of the application run as a daemon
  /// @return boolean
  static bool daemon() noexcept;

  /// @brief Directory of executables determined at runtime
  /// @return modifiable copy of std::filesystem::path
  static fs_path exec_dir() noexcept;

  /// @brief Should a running application be forcible restarted?
  /// @return boolean
  static bool force_restart() noexcept;

  /// @brief Git describe as determined at build time
  /// @return string
  static string git() noexcept;

  /// @brief Install prefix determined at build time by cmake
  /// @return modifiable copy of std::filesystem::path
  static fs_path install_prefix() noexcept;

  /// @brief Log file path detemined using cli args or build default
  /// @return modifiable std::filesystem::path
  static fs_path log_file() noexcept;

  /// @brief Parent directory of the invoked application
  /// @return modifiable std::filesystem::path
  static fs_path parent_dir() noexcept;

  /// @brief Full path of pid file (e.g. /run/pierre/pierre.pid) determined
  /// @return modifiable std::filesystem::path
  static fs_path pid_file() noexcept;
};

} // namespace conf
} // namespace pierre