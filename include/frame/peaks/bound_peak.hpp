// Pierre - Custom Light Show via DMX for Wiss Landing
// Copyright (C) 2021  Tim Hughey
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

#include "base/bound.hpp"
#include "base/conf/toml.hpp"
#include "frame/peaks/peak.hpp"

#include <fmt/format.h>

namespace pierre {

struct bound_peak : public bound<Peak> {
  friend fmt::formatter<bound_peak>;

  bound_peak() noexcept : bound{} {}
  bound_peak(Peak &&a, Peak &&b) noexcept
      : bound(std::array{std::forward<Peak>(a), std::forward<Peak>(b)}) {}
};

} // namespace pierre

template <> struct fmt::formatter<pierre::bound_peak> : public fmt::formatter<std::string> {

  template <typename FormatContext>
  auto format(const pierre::bound_peak &pb, FormatContext &ctx) const -> decltype(ctx.out()) {

    return fmt::format_to(ctx.out(), "[{} {}]", pb.first(), pb.second()); // write to ctx.out()
  }
};