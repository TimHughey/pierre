//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2023  Tim Hughey
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

#pragma once

#include "base/conf/toml.hpp"
#include "base/types.hpp"
#include "desk/color_part.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <fmt/format.h>
#include <iterator>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <vector>

namespace pierre {
namespace desk {

struct Hsb;

template <typename T>
concept IsRawRgb = (sizeof(T) >= 3) && (sizeof(T) <= 4) && std::integral<T>;

template <typename T>
concept HasScalingValues = requires(T v) {
  { v.scaling() } -> std::same_as<std::pair<double, double>>;
};

template <typename T>
concept IsColorScale = std::same_as<T, std::vector<Hsb>>;

template <typename T>
concept HasScaleToValue = std::convertible_to<T, double>;

struct Hsb {
  friend fmt::formatter<Hsb>;

  /// @brief Construct default (unstatured red)
  constexpr Hsb() = default;

  constexpr Hsb(const Hsb &) = default;
  constexpr Hsb(Hsb &&) = default;

  /// @brief Construct HSB from other and set part
  /// @tparam T Part type to set
  /// @param src HSB to use as the source
  /// @param part Part to set
  template <typename T>
    requires IsSpecializedColorPart<T>
  Hsb(Hsb src, T &&part) noexcept {
    src = part;
    *this = src;
  }

  /// @brief Create HSB color from RGB color code in 0xRRGGBB format
  /// @tparam T unsigned integral of more than three bytes
  /// @param rgb RGB color code
  template <typename T>
    requires IsRawRgb<T>
  Hsb(T rgb) noexcept {

    auto red{0.0};
    auto grn{0.0};
    auto blu{0.0};

    if constexpr (sizeof(T) >= 3 && std::integral<T>) {
      red = ((rgb & 0xf00) >> 16) / 255.0;
      grn = ((rgb & 0x0f0) >> 8) / 255.0;
      blu = (rgb & 0x00f) / 255.0;
    } else if constexpr (std::same_as<T, std::initializer_list<uint8_t>>) {
      auto it = rgb.begin();

      red = *it / 255.0;
      grn = *std::next(it) / 255.0;
      blu = *std::next(it) / 255.0;
    }

    // convert to hsb
    auto chroma_max = std::max(std::max(red, grn), blu);
    auto chroma_min = std::min(std::min(red, grn), blu);
    auto chroma_delta = chroma_max - chroma_min;

    if (chroma_delta > 0) {
      if (chroma_max == red)
        hue = Hue(60.0 * (std::fmod(((grn - blu) / chroma_delta), 6.0)));
      else if (chroma_max == grn)
        hue = Hue(60.0 * (((blu - red) / chroma_delta) + 2.0));
      else if (chroma_max == blu)
        hue = Hue(60.0 * (((red - grn) / chroma_delta) + 4.0));

      if (chroma_max > 0) sat = Sat(chroma_delta / chroma_max);

      bri = Bri(chroma_max);
    } else {
      bri = Bri(chroma_max);
    }

    if (hue < 0.0_HUE) hue += 360.0_HUE;
  }

  /// @brief Create HSB color from hue, saturation and brightness components
  ///        See https://tinyurl.com/tlhhsv2 for reference
  ///
  /// @param h Hue, 0-360˚ (0˚ = red, 120˚ = grn, 240˚ = blu, 360˚ = red)
  /// @param s Saturation, 0.0-1.0 or 0-100%
  /// @param b Brightness, 0.0-1.0 or 0-100%
  Hsb(desk::Hue &&h, desk::Sat &&s, desk::Bri &&b) noexcept
      : hue(std::forward<Hue>(h)), sat(std::forward<Sat>(s)), bri(std::forward<Bri>(b)) {}

  Hsb(const toml::table &t) noexcept { assign(t); }

  Hsb &operator=(const Hsb &) = default;
  Hsb &operator=(Hsb &&) = default;

  /// @tparam T hue_t, saturation_t, brightness_t
  /// @param part The part to assign
  /// @return Reference to changed object
  template <typename T>
    requires IsSpecializedColorPart<T>
  constexpr Hsb &operator=(const T &part) noexcept {

    if constexpr (std::same_as<T, Bri>) {
      bri = part;

    } else if constexpr (std::same_as<T, Hue>) {
      hue = part;

    } else if constexpr (std::same_as<T, Sat>) {
      sat = part;
    }

    return *this;
  }

  /// @brief Assign Hue, Sat and Bri from a configuration table
  /// @param forward
  void assign(const toml::table &t) noexcept {

    t.for_each([this](const toml::key &key, const toml::value<double> &tv) {
      auto v = tv.get();

      static constexpr auto kHue{"hue"};
      static constexpr auto kSat("sat"sv);
      static constexpr auto kBri("bri"sv);

      if (key == kHue) {
        hue = Hue(v);
      } else if (key == kSat) {
        sat = Sat(v);
      } else if (key == kBri) {
        bri = Bri(v);
      }
    });
  }

  template <typename T>
    requires IsSpecializedColorPart<T>
  void assign(T &&p) noexcept {

    if constexpr (IsColorPartHue<T>) {
      hue = Hue(std::forward<T>(p));
    } else if constexpr (IsColorPartSat<T>) {
      sat = Sat(std::forward<T>(p));
    } else {
      bri = Bri(std::forward<T>(p));
    }
  }

  constexpr bool black() const noexcept { return !visible(); }

  /// @brief Convert and copy the HSB color to a representative array
  ///        of RGB bytes including a fourth byte representing white
  ///        (for pinspots).
  ///
  ///        See https://tinyurl.com/tlhhsv for reference implementation
  ///
  /// @param span destination of frame bytes
  void copy_rgb_to(std::span<uint8_t> &&span) const noexcept {
    // handle when Hsb colors do not translate into
    // all three rgb bytes or if the destination is larger
    // than the RGB color
    std::ranges::fill(span, 0x00);

    /// NOTE: fix me by implementing normalize()
    auto hue0 = Hue(std::fmod(hue.get(), 360.0));
    auto bri0 = (bri > Bri::max()) ? bri / Bri::max() : bri;
    auto sat0 = (sat > Sat::max()) ? sat / Sat::max() : sat;

    // special case for conversion to RGB, must multiply bri and sat
    auto chroma = bri0.get() * sat0.get();

    /// NOTE: hue is already in the 0-360 range
    auto hue_prime = std::fmod(hue0.get() / 60.0, 6.0);

    // calculate what HSB "slot" should be used for the RGB color
    auto x = chroma * (1.0 - std::fabs(std::fmod(hue_prime, 2.0) - 1.0));
    auto m = bri0.get() - chroma;

    // determine which HSB "slot" the RGB value maps from to chose which
    // calculated values are placed into the individual components of RGB
    auto in_slot = [](auto hue_prime, auto s) -> bool {
      return (s <= hue_prime) && (hue_prime < (s + 1));
    };

    // helper to calc and populate a single RGB byte based on slot
    // decision from in_slot()
    enum __byte_num : uint8_t { Red = 0, Grn, Blu, White };
    auto store = [&](__byte_num c, auto &v) {
      span.data()[c] = static_cast<uint8_t>(std::round((v + m) * 255.0));
    };

    if (in_slot(hue_prime, 0)) {
      store(Red, chroma);
      store(Grn, x);
    } else if (in_slot(hue_prime, 1)) {
      store(Red, x);
      store(Grn, chroma);
    } else if (in_slot(hue_prime, 2)) {
      store(Grn, chroma);
      store(Blu, x);
    } else if (in_slot(hue_prime, 3)) {
      store(Grn, x);
      store(Blu, chroma);
    } else if (in_slot(hue_prime, 4)) {
      store(Red, x);
      store(Blu, chroma);
    } else if (in_slot(hue_prime, 5)) {
      store(Red, chroma);
      store(Blu, x);
    }
  }

  /// @brief Set the HSB color to black (all zeros)
  void dark() noexcept {
    hue.clear();
    sat.clear();
    bri.clear();
  }

  /// @brief Interpolate one color to another using a percentage that
  ///        represents the amount traveled between the two colors
  /// @param a Origin color
  /// @param b Destination color
  /// @param t Percentage traveled between the two
  /// @return HSB color
  static Hsb interpolate(Hsb a, Hsb b, double t) noexcept {

    // what distance will we interpolate?  assume, for now, that
    // color b comes after color a and this is an ascending interpolation
    auto d = b.hue - a.hue;

    // if color a comes after color b then reverse the colors and setup
    // for a descending interpolation
    if (a.hue > b.hue) {
      std::swap(a.hue, b.hue);

      d = Hue((double)d * -1.0);
      t = 1.0 - t;
    }

    desk::Hue hue;
    if (d > 0.5_HUE) {

      // handle bottom half of 360˚ and use 1.0 (360˚) as our reference
      a.hue += 1.0_HUE;
      hue = Hue(std::fmod((double)((a.hue + (b.hue - a.hue)) * static_cast<Hue>(t)), 1.0));
    } else if (d <= 0.5_HUE) {

      // handle top half of 360˚ and use 0 has our hue reference
      hue = Hue(a.hue + Hue(t) * d);
    }

    return Hsb(std::move(hue), a.sat * static_cast<Sat>(t) * (b.sat - a.sat),
               a.bri * static_cast<Bri>(t) * (b.bri - a.bri));
  }

  /*constexpr T interpolate(const auto &bpair, const T val) const noexcept {
    const T range_a = max() - min();
    const T range_b = bpair.max() - bpair.min();

    return (((val - min()) * range_b) / range_a) + bpair.min();
  }*/

  /// @brief Enable all comparison operators to color parts
  /// @tparam T color_part to compare
  /// @param comp color_part value (rhs)
  /// @return sames as double <=>
  template <typename T>
    requires IsSpecializedColorPart<T>
  auto operator<=>(const T &comp) const {
    if constexpr (std::same_as<T, Hue>) {
      return hue <=> comp;
    } else if constexpr (std::same_as<T, Sat>) {
      return sat <=> comp;
    } else {
      return bri <=> comp;
    }
  }

  /// @brief Enable all comparison operators for Hsb
  ///        NOTE: Hue, Sat and Bri are compared and therefore
  ///        it is recommended to use <=, >= for cases where
  ///        only a single component has a value other than zero
  /// @param rhs Hsb to compare
  /// @return std::partial_ordering
  constexpr auto operator<=>(const Hsb &rhs) const = default;

  operator bool() = delete;

  constexpr explicit operator desk::Bri() const noexcept { return bri; }
  constexpr explicit operator desk::Hue() const noexcept { return hue; }
  constexpr explicit operator desk::Sat() const noexcept { return sat; }

  template <typename T>
    requires IsSpecializedColorPart<T>
  T &part() noexcept {
    if constexpr (IsColorPartHue<T>) {
      return hue;
    } else if constexpr (IsColorPartSat<T>) {
      return sat;
    } else if constexpr (IsColorPartBri<T>) {
      return bri;
    } else {
      static_assert(AlwaysFalse<T>, "unkonwn part");
    }
  }

  template <typename T>
    requires IsSpecializedColorPart<T>
  Hsb &rotate(const T &step) noexcept {

    if constexpr (IsColorPartHue<T>) {
      hue.rotate(step);
    } else if constexpr (IsColorPartSat<T>) {
      sat.rotate(step);
    } else if constexpr (IsColorPartBri<T>) {
      bri.rotate(step);
    }

    return *this;
  }

  constexpr bool visible() const noexcept { return bri > 0.0_BRI; }

  void write_metric() const noexcept;

public:
  desk::Hue hue{0.0};
  desk::Sat sat{0.0};
  desk::Bri bri{0.0};
};

} // namespace desk

} // namespace pierre

namespace {
namespace desk = pierre::desk;
}

template <> struct fmt::formatter<desk::Hsb> {
  // Presentation format: h=hsb, r=rgb, empty=both
  bool hsb{false};
  bool rgb{false};

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {

    auto it = ctx.begin(), end = ctx.end();

    while ((it != end) && (*it != '}')) {
      if (*it == 'h') hsb = true;
      if (*it == 'r') rgb = true;
      it++;
    }

    if (!hsb && !rgb) hsb = rgb = true;

    // Return an iterator past the end of the parsed range:
    return it;
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const desk::Hsb &h, FormatContext &ctx) const -> decltype(ctx.out()) {

    std::string msg;
    auto w = std::back_inserter(msg);

    if (hsb) fmt::format_to(w, "hsb({} {} {})", h.hue, h.sat, h.bri);

    if (rgb) {
      if (hsb) fmt::format_to(w, " ");

      std::array<uint8_t, 4> parts;
      h.copy_rgb_to(parts);

      fmt::format_to(w, "rgbw(0x{:02x}{:02x}{:02x} {:02x})", parts[0], parts[1], parts[2],
                     parts[3]);
    }

    // write to the output iterator ctx.out()
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};