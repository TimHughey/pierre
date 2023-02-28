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
#include "base/clock_now.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <iterator>

namespace pierre {

string pet::humanize(Nanos d) {
  using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;
  string msg;
  auto w = std::back_inserter(msg);

  auto append = [&w](auto d) { fmt::format_to(w, "{} ", d); };

  if (auto x = as<Hours>(d); x != Hours::zero()) {
    append(x);
    d -= as<Nanos>(x);
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
  fmt::format_to(w, "{:0.2}", ms);

  return msg;
}

Nanos pet::monotonic() noexcept { return Nanos{clock_now::ns_raw(CLOCK_MONOTONIC_RAW)}; } // static
Nanos pet::realtime() noexcept { return Nanos{clock_now::ns_raw(CLOCK_REALTIME)}; }       // static
Nanos pet::boottime() noexcept { return Nanos{clock_now::ns_raw(CLOCK_BOOTTIME)}; }       // static

} // namespace pierre