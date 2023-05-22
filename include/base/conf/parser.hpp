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

#include "base/conf/toml.hpp"
#include "base/types.hpp"

#include <array>

namespace pierre {
namespace conf {

/// @brief Parse the config file as specified by global cli args. The parsed
///        file is 'merged' into the provided container reference.
///        Parse error messages are placed into the specified container.
/// @tparam Msgs container type to accept error messages
/// @tparam Table container type (e.g. toml::table)
/// @param tt_dest reference to the container for the parsed config
/// @param msgs container of error messages (if any). container is cleared
///             upon entry.
/// @return boolean indicating success or failure
template <typename Table, typename Msgs> inline bool parse(Table &tt_dest, Msgs &msgs) noexcept {
  msgs[Parser].clear();

  auto pt_result = toml::parse_file(fixed::cfg_file());

  if (pt_result && (pt_result.table() != tt_dest)) {
    tt_dest = pt_result.table(); // copy the table at root

  } else {
    msgs[Parser] = pt_result.error().description();
  }

  return msgs[Parser].empty();
}

} // namespace conf
} // namespace pierre