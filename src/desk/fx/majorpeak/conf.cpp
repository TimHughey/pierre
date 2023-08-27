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

#include "desk/fx/majorpeak/conf.hpp"
#include "base/logger.hpp"

namespace pierre {

namespace conf {

string majorpeak::format() const noexcept {

  std::string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "{} {:h} {}\n", pierre::dura::humanize(silence_timeout), base_color, spl_bound);

  for (const auto &color_spec : color_specs) {
    fmt::format_to(w, "{}color_spec {}\n", logger->indent_str(), color_spec);
  }

  for (const auto &spot_spec : spot_specs) {
    fmt::format_to(w, "{}spot_spec  {}\n", logger->indent_str(), spot_spec);
  }

  return msg;
}
} // namespace conf
} // namespace pierre
