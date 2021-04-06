/*
    devs/pinspot/color.hpp - Ruth Pin Spot Color
    Copyright (C) 2020  Tim Hughey

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

#ifndef pierre_lightdesk_color_hpp
#define pierre_lightdesk_color_hpp

#include <algorithm>
#include <cmath>
#include <ctgmath>
#include <string>

#include "misc/minmax.hpp"

namespace pierre {
namespace lightdesk {

class Hsb {
public:
  Hsb() = default;
  Hsb(const float hue, const float sat, const float bri);
  ~Hsb() = default;

  float hue() const { return _hue; }
  float saturation() const { return _sat; }
  float brightness() const { return _bri; }

  bool operator==(const Hsb &rhs) const;

  static Hsb fromRgb(uint32_t rgb_val);
  static Hsb fromRgb(uint8_t red, uint8_t grn, uint8_t blu);
  void toRgb(uint8_t &red, uint8_t &grn, uint8_t &blu) const;

public:
  // technically the default is unsaturated completely dark red
  float _hue = 0.0;
  float _sat = 0.0;
  float _bri = 0.0;
};

class Color {
public:
  using string = std::string;

  typedef uint8_t White;

public:
  Color() = default;
  ~Color() = default;

  Color(const uint rgb_val);
  Color(const Hsb &hsb);
  Color(const Color &rhs) = default;

  void copyRgbToByteArray(uint8_t *array) const;

  // components
  float brightness() const { return _hsb.brightness() * 100.0f; }
  float hue() const { return _hsb.hue() * 360.0f; }
  float saturation() const { return _hsb.saturation() * 100.0f; }

  Hsb hsb() { return _hsb; }

  static Color interpolate(Color a, Color b, float t);
  bool isBlack() const;
  bool isWhite() const;
  bool notBlack() const { return isBlack() == false; }
  bool notWhite() const { return isWhite() == false; }

  bool operator==(const Color &rhs) const;
  bool operator!=(const Color &rhs) const;

  Color &rotateHue(const float step = 1.0);

  Color &setBrightness(float val);
  Color &setBrightness(const Color &rhs);
  Color &setBrightness(const MinMaxFloat &range, const float val);

  Color &setHue(float val);

  Color &setSaturation(float val);
  Color &setSaturation(const Color &rhs);
  Color &setSaturation(const MinMaxFloat &range, const float val);

  // useful static colors
  static Color full() {
    Color color(0xffffff);
    color._white = 255;

    return std::move(color);
  }
  static Color black() { return std::move(Color()); }
  static Color none() { return std::move(Color()); }

  string asString() const;

private:
  Hsb _hsb;
  White _white = 0;
};

} // namespace lightdesk
} // namespace pierre

#endif
