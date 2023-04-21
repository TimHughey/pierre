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

#include "base/conf/getv.hpp"
#include "base/conf/master.hpp"

namespace pierre {
namespace conf {

// local path helpers
static toml::path mpb(auto raw) noexcept { return toml::path(root::build).append(raw); }
static toml::path mpc(auto raw) noexcept { return toml::path(root::cli).append(raw); }

// getv API
const toml::table *getv::mt() noexcept { return mptr->ttable.as<toml::table>(); }

// getv access API
const string getv::app_name() noexcept { return mptr->ttable[mpc(key::app_name)].ref<string>(); }
const string getv::build_vsn() noexcept {
  return mptr->ttable[mpb(key::git_describe)].ref<string>();
}

bool getv::daemon() noexcept { return mptr->ttable[mpc(key::daemon)].value_or(false); }

const string getv::data_dir() noexcept {
  return mptr->ttable[mpb(key::app_data_dir)].ref<string>();
}

bool getv::force_restart() noexcept { return mptr->ttable[mpc(key::force_restart)].ref<bool>(); }

const string getv::pid_file() noexcept {
  return mptr->ttable[mpc(key::pid_file_path)].ref<string>();
}

} // namespace conf
} // namespace pierre
