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
#include "frame/peaks/frequency.hpp"
#include "frame/peaks/magnitude.hpp"
#include "frame/peaks/types.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ranges>

namespace pierre {

struct Peak {
public:
  Peak() noexcept : freq(0), mag(0) {}
  Peak(const Frequency f, const Magnitude m) noexcept : freq(f), mag(m) {}

  auto frequency() const noexcept { return freq; }
  auto magnitude() const noexcept { return mag; }

  bool operator!() const noexcept { return (freq == 0) && (mag == 0); }

  friend std::strong_ordering operator<=>(const auto &lhs, const Frequency &freq) noexcept {
    return lhs.freq <=> freq;
  }

  friend std::strong_ordering operator<=>(const auto &lhs, const Magnitude &mag) noexcept {
    return lhs.mag <=> mag;
  }

  bool useable() const noexcept; // see .cpp, requires PeakConfig
  bool useable(const mag_min_max &ml) const noexcept { return ml.inclusive(mag); }
  bool useable(const mag_min_max &ml, const freq_min_max &fl) const noexcept {
    return ml.inclusive(mag) && fl.inclusive(freq);
  }

  static Peak zero() noexcept { return Peak(); }

private:
  Frequency freq{0};
  Magnitude mag{0};
};

} // namespace pierre

#include <fmt/format.h>

template <> struct fmt::formatter<pierre::Peak> {
  // Presentation format: 'f' - frequency, 'm' - magnitude, 'empty' - both
  bool freq = false;
  bool mag = false;

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {

    auto it = ctx.begin(), end = ctx.end();

    while ((it != end) && (*it != '}')) {
      if (*it == 'f') freq = true;
      if (*it == 'm') mag = true;
      it++;
    }

    if (!freq && !mag) {
      freq = mag = true;
    }

    // Return an iterator past the end of the parsed range:
    return it;
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const pierre::Peak &p, FormatContext &ctx) const -> decltype(ctx.out()) {

    std::vector<std::string> parts;
    if (freq) parts.emplace_back(fmt::format("freq={:>8.2f}", p.frequency()));
    if (mag) parts.emplace_back(fmt::format("mag={:>2.5f}", p.magnitude()));

    // ctx.out() is an output iterator to write to.
    return fmt::format_to(ctx.out(), "{}", fmt::join(parts, " "));
  }
};
