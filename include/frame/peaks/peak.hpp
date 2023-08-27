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
#include "base/input_info.hpp"
#include "base/types.hpp"
#include "frame/peaks/peak_part.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <ranges>
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
///        a series of freq/mag/spl representing
///        the sample's composition (e.g. major peak freq).
///
///        Each audio packet becomes 1024 freq/spl/mag pairs
///        and this object represents one of those pairs.
///        The 1024 individual Peak is stored, in descending
///        order by mag, in the container "Peaks".
struct Peak {

  constexpr Peak() = default;
  constexpr Peak(const peak::Freq f, const peak::Mag m) noexcept : freq(f), mag(m) { calc_spl(); }

  void assign(const toml::table &t) noexcept {
    // this for_each filters on only doubles, other keys are are ignored
    t.for_each([this](const toml::key &key, const toml::value<double> &val) {
      auto v = val.get(); // we can just get the toml::value

      if (key == "freq") {
        freq.assign(v);
      } else if (key == "mag") {
        mag.assign(v);

      } else if (key == "spl") {
        spl.assign(v);
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
    auto a = static_cast<T>(bp.first());
    auto b = static_cast<T>(bp.second());
    auto v = static_cast<T>(*this);

    return (v >= a) && (v <= b);
  }

  bool operator!() const = delete;
  operator bool() const = delete;

  // constexpr explicit operator bool() const noexcept { return (bool)freq && (bool)db; }
  constexpr explicit operator peak::Freq() const noexcept { return freq; }
  constexpr explicit operator peak::Spl() const noexcept { return spl; }

  /// @brief Enable all comparison operators for peak parts
  /// @param  peak_part right hand side
  /// @return same as double <=>
  constexpr auto operator<=>(const Peak &) const = default;

  template <typename T>
    requires IsSpecializedPeakPart<T>
  constexpr T part() const noexcept {
    if constexpr (IsPeakPartFrequency<T>) {
      return freq;
    } else if constexpr (IsPeakPartMag<T>) {
      return mag;
    } else if constexpr (IsPeakPartSpl<T>) {
      return spl;
    }
  }

  constexpr auto useable() const noexcept { return mag > peak::Mag(15); }

  /// @brief Determine if a peak is usable using a pair of min/max values
  /// @tparam T Component type of Peak to use for comparison (e.g. freq, Spl)
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
    } else if constexpr (IsPeakPartMag<T>) {
      return mag;
    } else if constexpr (IsPeakPartSpl<T>) {
      return spl;
    }
  }

private:
  // constexpr void calc_dB() noexcept {
  // https://www.eevblog.com/forum/beginners/how-to-interpret-the-magnitude-of-fft/
  // auto power = std::pow(2.0, InputInfo::bit_depth) / 2.0;
  // auto dB_n = 20.0 * (std::log10(mag.get()) - std::log10(InputInfo::spf * power));
  //
  //  https://www.quora.com/What-is-the-maximum-allowed-audio-amplitude-on-the-standard-audio-CD
  //  db = peak::dB(dB_n + 96.0);
  // }

  constexpr void calc_spl() noexcept { spl = peak::Spl(20.0 * std::log10(mag.get() / 20.0)); }

public:
  peak::Freq freq{};
  peak::Mag mag{};
  peak::Spl spl{};
};

} // namespace pierre

#include <fmt/format.h>
#include <variant>

namespace {
using namespace pierre;
}

template <> struct fmt::formatter<pierre::Peak> {

  // Presentation format:
  //   'f'  = frequency
  //   'm'  = mag
  //   's'  = spl
  //   none = f,d

  enum fmt_parts : uint8_t { Freq = 0, Mag, Spl };

  struct fmt_ctrl {
    char flag;
    bool want;
  };

  std::array<fmt_ctrl, 4> part_fmt{{{'f', false}, {'m', false}, {'s', false}}};

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    auto fc_end = part_fmt.end();

    while ((it != end) && (*it != '}')) {
      if (auto fc_it = std::ranges::find(part_fmt, *it, &fmt_ctrl::flag); fc_it != fc_end) {
        fc_it->want = true;
      }

      it++;
    }

    if (auto def = std::ranges::none_of(part_fmt, &fmt_ctrl::want); def) {
      part_fmt[Freq].want = part_fmt[Spl].want = true;
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

    if (part_fmt[Freq].want) fmt::format_to(w, "{:0.0f}Hz ", p.freq);
    if (part_fmt[Mag].want) fmt::format_to(w, "{:0.1f}mag ", p.mag);
    if (part_fmt[Spl].want) fmt::format_to(w, "{:0.1f}spl", p.spl);

    // ctx.out() is an output iterator
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};
