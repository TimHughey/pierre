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

#pragma once

#include "base/min_max_pair.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <cmath>
#include <ctgmath>
#include <fmt/format.h>
#include <string>
#include <type_traits>

namespace pierre {

struct Hsb {
  // no constructor -- aggregate initialization

  bool operator==(const Hsb &rhs) const;

  static Hsb fromRgb(uint32_t rgb_val);
  static Hsb fromRgb(uint8_t red, uint8_t grn, uint8_t blu);
  void toRgb(uint8_t &red, uint8_t &grn, uint8_t &blu) const;

  // technically the default is unsaturated completely dark red
  double hue = 0.0;
  double sat = 0.0;
  double bri = 0.0;
};

class Color {
public:
  typedef uint8_t White;

public:
  Color() = default;
  ~Color() = default;

  Color(const uint rgb_val);
  Color(const Hsb &hsb);
  Color(const Color &rhs) = default;

  void copyRgbToByteArray(uint8_t *array) const;

  // components
  double brightness() const { return _hsb.bri * 100.0f; }
  double hue() const { return _hsb.hue * 360.0f; }
  double saturation() const { return _hsb.sat * 100.0f; }

  // colorspace
  const Hsb &hsb() const noexcept { return _hsb; }

  static Color interpolate(Color a, Color b, double t);
  bool isBlack() const;
  bool isWhite() const;
  bool notBlack() const { return isBlack() == false; }
  bool notWhite() const { return isWhite() == false; }

  bool operator==(const Color &rhs) const;
  bool operator!=(const Color &rhs) const;

  Color &rotateHue(const double step = 1.0);

  template <class T> Color &setBrightness(T &&val) {
    if constexpr (std::is_floating_point<T>::type::value) {
      _hsb.bri = val / 100.0;
    }

    if constexpr (std::is_same_v<T, Color>) {
      _hsb.bri = val._hsb.bri;
    }

    return *this;
  }

  template <class T> auto setBrightness(const T &range, const double val) -> Color & {
    T brightness_range(0.0f, brightness());
    return setBrightness(range.interpolate(brightness_range, val));
  }

  Color &setHue(double val);

  Color &setSaturation(double val);
  Color &setSaturation(const Color &rhs);
  Color &setSaturation(const min_max_pair<double> &range, const double val);

  // useful static colors
  static Color full() {
    Color color(0xffffff);
    color._white = 255;

    return color;
  }
  static constexpr Color black() { return std::move(Color()); }
  static constexpr Color none() { return std::move(Color()); }

  // string asString() const;

private:
  Hsb _hsb;
  White _white = 0;
};

namespace color {
constexpr Color NONE = Color::none();
}

} // namespace pierre

template <> struct fmt::formatter<pierre::Color> {
  // Presentation format: h=hsb, r=rgb, empty=both
  bool hsb = false;
  bool rgb = false;

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {

    auto it = ctx.begin(), end = ctx.end();

    while ((it != end) && (*it != '}')) {
      if (*it == 'h') hsb = true;
      if (*it == 'r') rgb = true;
      it++;
    }

    if (!hsb && !rgb) {
      hsb = rgb = true;
    }

    // Return an iterator past the end of the parsed range:
    return it;
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const pierre::Color &c, FormatContext &ctx) const -> decltype(ctx.out()) {

    std::string msg;
    auto w = std::back_inserter(msg);

    if (hsb) {
      fmt::format_to(w, "hsb({:7.02f} {:5.1f} {:5.1f})", c.hue(), c.saturation(), c.brightness());
    }

    if (rgb) {
      uint8_t red, grn, blu = 0;
      c.hsb().toRgb(red, grn, blu);

      fmt::format_to(w, "{}rgb({:02x} {:02x} {:02x})", hsb ? " " : "", red, grn, blu);
    }

    // ctx.out() is an output iterator to write to.
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};
