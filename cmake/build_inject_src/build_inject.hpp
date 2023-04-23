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

#include <filesystem>
#include <string>
#include <string_view>

namespace pierre {
namespace build {

using fs_path = std::filesystem::path;
using string = std::string;
using string_view = std::string_view;

struct info_t {
  const string project;
  const string git;
  const fs_path install_prefix;
  const fs_path bin_dir;
  const fs_path libexec;
  const fs_path sysconf_dir;
  const fs_path data_dir;
  const fs_path state_dir;
};

extern const info_t info;

} // namespace build
} // namespace pierre
