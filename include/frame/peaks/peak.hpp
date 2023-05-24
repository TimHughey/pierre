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

#include "base/min_max_pair.hpp"
#include "base/types.hpp"
#include "frame/peaks/frequency.hpp"
#include "frame/peaks/magnitude.hpp"

// #include <algorithm>
#include <fmt/format.h>

namespace pierre {

using freq_min_max = min_max_pair<Frequency>;
using mag_min_max = min_max_pair<Magnitude>;

/// @brief A Peak is the output of the audio data FFT.
///        Each "packet" of audio PCM data consists of
///        2048 samples (two channels of 1024 32-bit bytes).
///        The FFT transforms the audio sameples into
///        a series of freq/mags representing
///        the sample's composition (e.g. major peak freq).
///
///        Each audio packet becomes 1024 freq/mag pairs
///        and this object represents one of those pairs.
///        The 1024 individual Peak is stored, in descending
///        order by mag, in the container "Peaks".
struct Peak {
public:
  Peak() = default;
  Peak(const Frequency f, const Magnitude m) noexcept : freq(f), mag(m) {}

  /// @brief Is this peak silence (mag == 0, freq == 0)
  /// @return boolean
  bool operator!() const noexcept { return (freq == 0) && (mag == 0); }

  /// @brief Compare a Peak to a frequency
  /// @param rhs Frequency
  /// @return Varies depending on comparison
  constexpr std::partial_ordering operator<=>(const Frequency &rhs) const noexcept {
    if (freq < rhs) return std::partial_ordering::less;
    if (freq > rhs) return std::partial_ordering::greater;

    return std::partial_ordering::equivalent;
  }

  /// @brief Compare a Peak to a magnitude
  /// @param rhs Magnitude
  /// @return Varies depending on comparison
  constexpr std::partial_ordering operator<=>(const Magnitude &rhs) const noexcept {
    if (mag < rhs) return std::partial_ordering::less;
    if (mag > rhs) return std::partial_ordering::greater;

    return std::partial_ordering::equivalent;
  }

  /// @brief Is the Peak useable?
  ///        The Peak magnitude is compared to a mag min/max
  ///        and is useabled when between the min/max (inclusive)
  /// @return boolean
  bool useable() const noexcept { return useable(mag_min_max(0.9, 128.0)); }

  bool useable(const mag_min_max &ml) const noexcept { return ml.inclusive(mag); }
  bool useable(const mag_min_max &ml, const freq_min_max &fl) const noexcept {
    return ml.inclusive(mag) && fl.inclusive(freq);
  }

public:
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

  // Formats the Peak p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const pierre::Peak &p, FormatContext &ctx) const -> decltype(ctx.out()) {
    std::string msg;
    auto w = std::back_inserter(msg);

    if (freq) fmt::format_to(w, "freq={:>8.2f}", p.freq);
    if (mag) fmt::format_to(w, "mag={:>2.5f}", p.mag);

    // ctx.out() is an output iterator to write to.
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};
