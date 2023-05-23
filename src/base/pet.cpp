//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include "base/pet.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>

namespace pierre {

string pet::humanize(Nanos d) {
  string msg;
  auto w = std::back_inserter(msg);

  auto append = [&w](auto d) { fmt::format_to(w, "{} ", d); };

  if (auto x = as<Hours>(d); x != Hours::zero()) {
    append(x);
    d -= x;
  }

  if (auto x = as<Minutes>(d); x != Minutes::zero()) {
    append(x);
    d -= as<Nanos>(x);
  }

  if (auto x = as<Seconds>(d); x != Seconds::zero()) {
    append(x);
    d -= as<Nanos>(x);
  }

  auto ms = as<millis_fp>(d);
  if (ms > 0ms) {
    fmt::format_to(w, "{:0.2}", ms);
  } else {
    fmt::format_to(w, "0.0ms");
  }

  return msg;
}

} // namespace pierre