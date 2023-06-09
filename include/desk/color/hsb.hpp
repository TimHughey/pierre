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

#include <array>
#include <cmath>
#include <fmt/format.h>
#include <iterator>

namespace pierre {
namespace desk {

template <typename T>
concept IsRawRgb = (sizeof(T) >= 3) && (sizeof(T) <= 4) && std::integral<T>;

struct Hsb {
  friend fmt::formatter<Hsb>;

  /// @brief Construct default (unstatured red)
  Hsb() = default;

  Hsb(const Hsb &) = default;
  Hsb(Hsb &&) = default;

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
        _hue.assign(60.0 * (std::fmod(((grn - blu) / chroma_delta), 6.0)));
      else if (chroma_max == grn)
        _hue.assign(60.0 * (((blu - red) / chroma_delta) + 2.0));
      else if (chroma_max == blu)
        _hue.assign(60.0 * (((red - grn) / chroma_delta) + 4.0));

      if (chroma_max > 0) _sat.assign(chroma_delta / chroma_max);

      _bri.assign(chroma_max);
    } else {
      _bri.assign(chroma_max);
    }

    if (_hue < 0.0_HUE) _hue += 360.0_HUE;
  }

  /// @brief Create HSB color from hue, saturation and brightness components
  ///        See https://tinyurl.com/tlhhsv2 for reference
  ///
  /// @param h Hue, 0-360˚ (0˚ = red, 120˚ = grn, 240˚ = blu, 360˚ = red)
  /// @param s Saturation, 0.0-1.0 or 0-100%
  /// @param b Brightness, 0.0-1.0 or 0-100%
  // Hsb(desk::hue_t &&h, desk::saturation_t &&s, desk::brightness_t &&b) noexcept
  //     : hue{h}, sat{s}, bri{b} {

  //   hue.normalize();
  //   sat.normalize();
  //   bri.normalize();
  // }

  ///        See https://tinyurl.com/tlhhsv2 for reference
  ///
  /// @param h Hue, 0-360˚ (0˚ = red, 120˚ = grn, 240˚ = blu, 360˚ = red)
  /// @param s Saturation, 0.0-1.0 or 0-100%
  /// @param b Brightness, 0.0-1.0 or 0-100%
  Hsb(desk::Hue &&h, desk::Sat &&s, desk::Bri &&b) noexcept {

    _hue.assign(h);
    _sat.assign(s);
    _bri.assign(b);
  }

  Hsb(const toml::node &node) noexcept {
    if (node.is_table()) assign(node.as<toml::table>());
  }

  Hsb(toml::table *t) noexcept { assign(t); }

  Hsb &operator=(const Hsb &) = default;
  Hsb &operator=(Hsb &&) = default;

  /// @brief Assign a component part to the HSB color
  /// @tparam T hue_t, saturation_t, brightness_t
  /// @param part The part to assign
  /// @return Reference to changed object
  template <typename T>
    requires IsAnyOf<T, desk::Hue, desk::Sat, desk::Bri>
  auto &operator=(const T &part) noexcept {

    if constexpr (std::same_as<T, desk::Bri>) {
      _bri.assign(part);
    } else if constexpr (std::same_as<T, desk::Hue>) {
      _hue.assign(part);
    } else if constexpr (std::same_as<T, desk::Sat>) {
      _sat.assign(part);
    }

    return *this;
  }

  void assign(const toml::table *t) noexcept { assign(*t); }
  void assign(const toml::table &t) noexcept {
    auto make_val = [&t](auto &&key) { return t[key].value_or(0.0); };

    _hue.assign(make_val("hue"));
    _sat.assign(make_val("sat"));
    _bri.assign(make_val("bri"));
  }

  /// @brief Set the HSB color to black (all zeros)
  void black() noexcept {
    _hue.clear();
    _sat.clear();
    _bri.clear();
  }

  /// @brief Convert and copy the HSB color to a representative array
  ///        of RGB bytes including a fourth byte representing white
  ///        (for pinspots).
  ///
  ///        See https://tinyurl.com/tlhhsv for reference implementation
  ///
  /// @param rgb Reference to array to place bytes
  void copy_as_rgb(std::array<uint8_t, 4> &rgb) const noexcept {
    enum C : uint8_t { Red = 0, Grn, Blu, White };

    auto bri0 = (_bri > 1.0_BRI) ? _bri / 100.0_BRI : _bri;
    auto sat0 = (_sat > 1.0_SAT) ? _sat / 100.0_SAT : _sat;

    // special case for conversion to RGB, must multiply bri and sat
    auto chroma = static_cast<double>(bri0) * static_cast<double>(sat0);

    /// NOTE: hue is already in the 0-360 range
    auto hue_prime = std::fmod(static_cast<double>(_hue) / 60.0, 6.0);

    // calculate what "slot" the HSB color aligns to in the RGB space
    auto x = chroma * (1.0 - std::fabs(std::fmod(hue_prime, 2.0) - 1.0));
    auto m = static_cast<double>(bri0) - chroma;

    rgb.fill(0x00);

    // calculate a portion of the RGB value based on conversions above
    auto convert = [&](C c, auto &v) {
      rgb[c] = static_cast<uint8_t>(std::round((v + m) * 255.0));
    };

    // determine which HSB "slot" the RGB value maps into to chose which
    // values calculated are placed into the individual components of RGB
    auto in_slot = [&hue_prime](auto s) -> bool {
      return (s <= hue_prime) && (hue_prime < (s + 1));
    };

    if (in_slot(0)) {
      convert(Red, chroma);
      convert(Grn, x);
    } else if (in_slot(1)) {
      convert(Red, x);
      convert(Grn, chroma);
    } else if (in_slot(2)) {
      convert(Grn, chroma);
      convert(Blu, x);
    } else if (in_slot(3)) {
      convert(Grn, x);
      convert(Blu, chroma);
    } else if (in_slot(4)) {
      convert(Red, x);
      convert(Blu, chroma);
    } else if (in_slot(5)) {
      convert(Red, chroma);
      convert(Blu, x);
    }
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
    auto d = b._hue - a._hue;

    // if color a comes after color b then reverse the colors and setup
    // for a descending interpolation
    if (a._hue > b._hue) {
      std::swap(a._hue, b._hue);

      d *= -1.0;
      t = 1.0 - t;
    }

    desk::Hue hue;
    if (d > 0.5) {

      // handle bottom half of 360˚ and use 1.0 (360˚) as our reference
      a._hue += 1.0_HUE;
      hue.assign(std::fmod((double)((a._hue + (b._hue - a._hue)) * static_cast<Hue>(t)), 1.0));
    } else if (d <= 0.5) {

      // handle top half of 360˚ and use 0 has our hue reference
      hue.assign((double)a._hue + t * d);
    }

    return Hsb(std::move(hue), a._sat * static_cast<Sat>(t) * (b._sat - a._sat),
               a._bri * static_cast<Bri>(t) * (b._bri - a._bri));
  }

  /// @brief Add a fixed amount to hue, saturation or brightness
  ///        The type of step is used to automatically add the
  ///        step amount to the appropriate HSB component
  /// @tparam T Type of value to add
  /// @param step Amount to add
  /// @return Reference to the object changed
  template <typename T>
    requires IsSpecializedColorPart<T>
  Hsb &operator+=(T step) noexcept {
    if constexpr (std::same_as<T, desk::Hue>) {
      _hue += step;
    } else if constexpr (std::same_as<T, desk::Sat>) {
      _sat += step;
    } else if constexpr (std::same_as<T, desk::Bri>) {
      _bri += step;
    }

    return *this;
  }

  bool operator==(const Hsb &lhs) const noexcept {
    return (lhs._hue == _hue) && (lhs._sat == _sat) && (lhs._bri == _bri);
  }

  explicit operator bool() const { return (bool)_hue && (bool)_sat && (bool)_bri; }

  template <typename T>
    requires IsSpecializedColorPart<T>
  auto &rotate(const T &step) noexcept {

    if constexpr (std::same_as<T, desk::Hue>) {
      if (desk::Hue next_hue = _hue + step; next_hue.valid()) {
        std::swap(_hue, next_hue);
      } else {
        _hue.clear();
      }

    } else if constexpr (std::same_as<T, desk::Sat>) {
      if (desk::Sat next_sat = _sat + step; next_sat.valid()) {
        std::swap(_sat, next_sat);
      } else {
        _sat.clear();
      }

    } else if constexpr (std::same_as<T, desk::Bri>) {
      if (desk::Bri next_bri = _bri + step; next_bri.valid()) {
        std::swap(_bri, next_bri);
      } else {
        _bri.clear();
      }
    }

    return *this;
  }

  operator desk::Bri() const noexcept { return _bri; }
  operator desk::Hue() const noexcept { return _hue; }
  operator desk::Sat() const noexcept { return _sat; }

public:
  desk::Hue _hue;
  desk::Sat _sat;
  desk::Bri _bri;
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

    if (hsb) fmt::format_to(w, "hsb({} {} {})", h._hue, h._sat, h._bri);

    if (rgb) {
      if (hsb) fmt::format_to(w, " ");

      std::array<uint8_t, 4> parts;
      h.copy_as_rgb(parts);

      fmt::format_to(w, "rgbw(0x{:02x}{:02x}{:02x} {:02x})", parts[0], parts[1], parts[2],
                     parts[3]);
    }

    // write to the output iterator ctx.out()
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};