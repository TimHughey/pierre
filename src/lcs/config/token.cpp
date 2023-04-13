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

#define TOML_ENABLE_FORMATTERS 0 // don't need formatters
#define TOML_HEADER_ONLY 0       // reduces compile times

#include "lcs/config/token.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "toml++/toml.h"

namespace pierre {
namespace config2 {

token::~token() noexcept {
  if (watch_raw) watch_raw->unregister_token(*this);
}

void token::notify_of_change(std::any &&next_table) noexcept {
  static constexpr csv fn_id{"notify"};

  const auto &table0 = std::any_cast<toml::table>(table);
  const auto &table1 = std::any_cast<toml::table>(next_table);

  if (table0 != table1) {
    INFO_AUTO("mod_id={} table_size={}\n", mod_id, table1.size());
    const auto &h = std::any_cast<lambda>(handler);

    h(std::forward<std::any>(next_table));
  }
}

} // namespace config2
} // namespace pierre
