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

namespace pierre {
namespace conf {

struct root {
  static constexpr auto cli{"cli"};
  static constexpr auto build{"build"};
};

struct key {
  static constexpr auto app_name{"app-name"};

  static constexpr auto cfg_file{"cfg-file"};

  static constexpr auto daemon{"daemon"};
  static constexpr auto data_path{"data-path"};

  static constexpr auto dmx_host{"dmx-host"};
  static constexpr auto exec_dir{"exec-path"};
  static constexpr auto force_restart{"force-restart"};
  static constexpr auto git_describe{"git-describe"};
  static constexpr auto help{"help"};
  static constexpr auto log_file{"log-file"};
  static constexpr auto parent_dir{"parent-dir"};
  static constexpr auto pid_file_path{"pid-path"};
  static constexpr auto project{"project"};
  static constexpr auto install_dir{"install-dir"};
  static constexpr auto sysconf_dir{"sysconf-dir"};
  static constexpr auto app_data_dir{"data-dir"};
};

} // namespace conf
} // namespace pierre
