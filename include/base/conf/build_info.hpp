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
#include "build_inject.hpp"

namespace pierre {
namespace conf {

struct build_info {

  static const toml::table ttable() noexcept {

    return toml::table{{key::project, build::info.project},
                       {key::install_dir, build::info.install_prefix},
                       {key::sysconf_dir, build::info.sysconf_dir},
                       {key::app_data_dir, build::info.data_dir}};
  }
};

} // namespace conf
} // namespace pierre