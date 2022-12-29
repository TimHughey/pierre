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

#include "clock_info.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <iterator>

namespace pierre {

const string ClockInfo::inspect() const {
  Nanos now = pet::now_monotonic();
  string msg;
  auto w = std::back_inserter(msg);

  constexpr auto hex_fmt_str = FMT_STRING("{:>35}={:#x}\n");
  constexpr auto gen_fmt_str = FMT_STRING("{:>35}={}\n");

  fmt::format_to(w, hex_fmt_str, "clockId", clock_id);
  fmt::format_to(w, gen_fmt_str, "rawOffset", rawOffset);
  fmt::format_to(w, gen_fmt_str, "now_ns", pet::now_monotonic());
  fmt::format_to(w, gen_fmt_str, "mastershipStart", mastershipStartTime);
  fmt::format_to(w, gen_fmt_str, "sampleTime", sampleTime);
  fmt::format_to(w, gen_fmt_str, "master_for", pet::humanize(master_for(now)));
  fmt::format_to(w, gen_fmt_str, "sample_age", sample_age.humanize());

  return msg;
};

} // namespace pierre