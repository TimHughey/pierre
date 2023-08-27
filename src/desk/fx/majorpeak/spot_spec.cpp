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

#include "desk/fx/majorpeak/spot_spec.hpp"
#include "base/dura.hpp"
#include "base/logger.hpp"

#include <fmt/format.h>

namespace pierre {

namespace desk {

string spot_spec::format() const noexcept {
  std::string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "{:<8} {:<8} color_spec={} fade[{} {:h}]", id, unit, color_spec,
                 pierre::dura::humanize(fade.timeout), fade.color);

  for (const auto &alt : alternates) {
    fmt::format_to(w, "\n{}  alternate {}", logger->indent_str(), alt);
  }

  return msg;
}

string spot_spec::alternate::format() const noexcept {

  std::string msg;
  auto w = std::back_inserter(msg);
  constexpr std::array alt_list{alt_t::Greater, alt_t::Freq, alt_t::Spl, alt_t::Last};
  fmt::format_to(w, "{:h}", color);

  for (const auto a : alt_list) {
    fmt::format_to(w, " {}={} ", alt_desc(a), alts[a]);
  }

  return msg;
}
} // namespace desk
} // namespace pierre
