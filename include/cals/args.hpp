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

struct ArgsMap {
  bool parse_ok{false};
  bool help{false};
  bool daemon{false};
  std::filesystem::path exec_path{};
  std::filesystem::path parent_path{};
  string cfg_file{};
  string dmx_host{};
  string pid_file{};
  string app_name;

  // public api
  bool ok() const { return parse_ok; }
};

class Args {
public:
  Args() = default;

  ArgsMap parse(int argc, char *argv[]);

public:
  static constexpr csv module_id{"ARGS"};
};
} // namespace pierre