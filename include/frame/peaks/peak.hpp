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

#include "base/bound.hpp"
#include "base/conf/toml.hpp"
#include "base/types.hpp"
#include "frame/peaks/peak_part.hpp"

// #include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <fmt/format.h>
#include <tuple>

namespace pierre {

using bound_freq = bound<peak::Freq>;

template <typename T>
concept IsPeakBoundedRange = requires(T v) {
  { v.first() };
  { v.second() };
};

/// @brief A Peak is the output of the audio data FFT.
///        Each "packet" of audio PCM data consists of
///        2048 samples (two channels of 1024 32-bit bytes).
///        The FFT transforms the audio sameples into
///        a series of freq/dB representing
///        the sample's composition (e.g. major peak freq).
///
///        Each audio packet becomes 1024 freq/dB pairs
///        and this object represents one of those pairs.
///        The 1024 individual Peak is stored, in descending
///        order by dB, in the container "Peaks".
struct Peak {

  constexpr Peak() = default;
  constexpr Peak(const peak::Freq f, const peak::dB db) noexcept : freq(f), db(db) {}

  constexpr Peak(peak::dB &&db) noexcept : freq(0.0), db(std::forward<peak::dB>(db)) {}

  void assign(const toml::table &t) noexcept {
    // this for_each filters on only doubles, other keys are are ignored
    t.for_each([this](const toml::key &key, const toml::value<double> &val) {
      auto v = val.get(); // we can just get the toml::value

      if (key == "freq") {
        freq.assign(v);
      } else if (key == "dB") {
        db.assign(v);
      }
    });
  }

  template <typename T>
    requires IsSpecializedPeakPart<T>
  constexpr auto inclusive(const Peak &a, const Peak &b) const {
    auto v = static_cast<T>(*this);

    return (v >= static_cast<T>(a)) && (v <= static_cast<T>(b));
  }

  template <typename T, typename U>
    requires IsSpecializedPeakPart<T> && IsPeakBoundedRange<U>
  auto inclusive(const U &bp) const {
    auto v = static_cast<T>(*this);
    auto a = static_cast<T>(bp.first());
    auto b = static_cast<T>(bp.second());

    return (v >= a) && (v <= b);
  }

  bool operator!() const = delete;
  operator bool() const = delete;

  // constexpr explicit operator bool() const noexcept { return (bool)freq && (bool)db; }
  constexpr explicit operator peak::Freq() const noexcept { return freq; }
  constexpr explicit operator peak::dB() const noexcept { return db; }

  /// @brief Enable all comparison operators for peak parts
  /// @param  peak_part right hand side
  /// @return same as double <=>
  constexpr auto operator<=>(const Peak &) const = default;

  template <typename T>
    requires IsSpecializedPeakPart<T>
  constexpr T part() const noexcept {
    if constexpr (IsPeakPartFrequency<T>) {
      return freq;
    } else if constexpr (isPeakPartdB<T>) {
      return db;
    }
  }

  constexpr auto useable() const noexcept { return db > peak::dB(); }

  /// @brief Determine if a peak is usable using a pair of min/max values
  /// @tparam T Component type of Peak to use for comparison (e.g. freq, dB)
  /// @tparam U Type of min/max values (must match T)
  /// @param br
  /// @return
  template <typename T, typename U>
    requires IsSpecializedPeakPart<T> && IsPeakBoundedRange<U>
  constexpr auto useable(const U &br) const noexcept {
    return inclusive<T>(br);
  }

  template <typename T>
    requires IsSpecializedPeakPart<T>
  constexpr const T &val() const noexcept {
    if constexpr (IsPeakPartFrequency<T>) {
      return freq;
    } else if constexpr (isPeakPartdB<T>) {
      return db;
    }
  }

public:
  peak::Freq freq{};
  peak::dB db{};
};

} // namespace pierre

#include <fmt/format.h>

template <> struct fmt::formatter<pierre::Peak> {
  // Presentation format: 'f' - frequency, 'd' - dB, 'empty' - both
  bool want_freq = false;
  bool want_dB = false;

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {

    auto it = ctx.begin(), end = ctx.end();

    while ((it != end) && (*it != '}')) {
      if (*it == 'f') want_freq = true;
      if (*it == 'd') want_dB = true;
      it++;
    }

    if (!want_freq && !want_dB) {
      want_freq = want_dB = true;
    }

    // Return an iterator past the end of the parsed range:
    return it;
  }

  // Formats the Peak using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const pierre::Peak &p, FormatContext &ctx) const -> decltype(ctx.out()) {
    std::string msg;
    auto w = std::back_inserter(msg);

    if (want_freq) fmt::format_to(w, "{:>8.0f}Hz ", p.freq);
    if (want_dB) fmt::format_to(w, "{:>02.0f}dB", p.db);

    // ctx.out() is an output iterator to write to.
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};
