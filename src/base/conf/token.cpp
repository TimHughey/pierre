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

#define TOML_IMPLEMENTATION

#include "conf/token.hpp"
#include "conf/cli_args.hpp"
#include "conf/fixed.hpp"

#include <filesystem>
#include <fmt/format.h>

namespace pierre {
namespace conf {

const string token::make_path() noexcept {
  const auto base = root.begin()->key();
  return fixed::cfg_path().append(base).replace_extension(".toml");
}

bool token::parse() noexcept {

  msgs[Parser].clear();

  // parse file at this path
  const auto base = root.begin()->key();
  auto f_path = make_path();

  auto pt_result = toml::parse_file(f_path.c_str());

  if (pt_result) {
    ttable = pt_result.table(); // copy the table at root

  } else {
    msgs[Parser] = pt_result.error().description();
  }

  msgs[Info] = fmt::format("{}", *this);

  return parse_ok();
}

} // namespace conf
} // namespace pierre
