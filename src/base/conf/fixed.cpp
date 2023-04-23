// Pierre
// Copyright (C) 2023 Tim Hughey
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

#include "conf/fixed.hpp"
#include "build_inject.hpp"
#include "conf/cli_args.hpp"
#include "conf/keys.hpp"

#include <filesystem>

namespace pierre {
namespace conf {

using fs_path = std::filesystem::path;

fs_path fixed::app_data_dir() noexcept { return build::info.data_dir; }

csv fixed::app_name() noexcept { return build::info.project; }

string fixed::cfg_dir() noexcept { return cli_args::ttable[key::cfg_dir].ref<string>(); }

fs_path fixed::cfg_path() noexcept {
  fs_path base = cli_args::ttable[key::cfg_path].ref<string>();

  return base.append(cfg_dir());
}

bool fixed::daemon() noexcept { return cli_args::ttable[key::daemon].ref<bool>(); }

fs_path fixed::exec_dir() noexcept { return cli_args::ttable[key::exec_dir].ref<string>(); }

bool fixed::force_restart() noexcept { return cli_args::ttable[key::force_restart].ref<bool>(); }

string fixed::git() noexcept { return build::info.git; }

fs_path fixed::install_prefix() noexcept { return build::info.install_prefix; }

fs_path fixed::log_file() noexcept { return cli_args::ttable[key::log_file].ref<string>(); }

fs_path fixed::parent_dir() noexcept { return cli_args::ttable[key::parent_dir].ref<string>(); }

fs_path fixed::pid_file() noexcept { return cli_args::ttable[key::pid_file].ref<string>(); }

} // namespace conf
} // namespace pierre