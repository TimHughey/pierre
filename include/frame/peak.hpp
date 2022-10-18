/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "base/frequency.hpp"
#include "base/helpers.hpp"
#include "base/magnitude.hpp"
#include "base/minmax.hpp"
#include "base/types.hpp"
#include "frame/peak_ref_data.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ranges>

namespace {
namespace ranges = std::ranges;
}

namespace pierre {

struct Peak {
public:
  static PeakMagBase mag_base;     // see peaks_ref_data.hpp and peaks.cpp
  static PeakMagScaled mag_scaled; // see peaks_ref_data.hpp and peaks.cpp

public: // Peak
  Peak() noexcept {};
  Peak(const Frequency f, const Magnitude m) noexcept : freq(f), mag(m) {}

  static min_max_float magScaleRange() {
    return min_max_float(0.0, mag_scaled.ceiling - mag_scaled.floor);
  }

  Frequency frequency() const { return freq; }
  Frequency frequencyScaled() const { return scale_val(freq); }
  bool greaterThanFloor() const { return mag_base.floor; }
  bool greaterThanFreq(Freq want_freq) const { return freq > want_freq; }

  auto magnitude() const { return mag; }

  MagScaled magScaled() const {
    auto scaled = scale_val(mag) - mag_scaled.floor;

    return scaled > 0 ? scaled : 0;
  }

  bool magStrong() const { return mag >= (mag_base.floor * mag_base.strong); }

  explicit operator bool() const {
    return (mag > mag_base.floor) && (mag < mag_base.ceiling) ? true : false;
  }

  friend std::strong_ordering operator<=>(const auto &lhs, const Frequency &freq) noexcept {
    return lhs.freq <=> freq;
  }

  friend std::strong_ordering operator<=>(const auto &lhs, const Magnitude &mag) noexcept {
    return lhs.mag <=> mag;
  }

  template <class T, class = typename std::enable_if<std::is_same<T, Mag>::value>::type>
  T scaled() const {
    auto x = scale_val(mag) - mag_scaled.floor;

    return x > 0 ? x : 0;
  }

  template <typename T> const T scaleMagToRange(const min_max_pair<T> &range) const {
    auto ret_val =
        static_cast<T>(mag_scaled.interpolate(mag) * (range.max() - range.min()) + range.min());

    return ranges::clamp(ret_val, range.min(), range.max());
  }

  static const Peak zero() { return Peak(); }

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

  // Parses format specifications of the form ['f' | 'e'].
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    // [ctx.begin(), ctx.end()) is a character range that contains a part of
    // the format string starting from the format specifications to be parsed,
    // e.g. in
    //
    //   fmt::format("{:f} - point of interest", point{1, 2});
    //
    // the range will contain "f} - point of interest". The formatter should
    // parse specifiers until '}' or the end of the range. In this example
    // the formatter should parse the 'f' specifier and return an iterator
    // pointing to '}'.

    // Please also note that this character range may be empty, in case of
    // the "{}" format string, so therefore you should check ctx.begin()
    // for equality with ctx.end().

    // Parse the presentation format and store it in the formatter:
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
