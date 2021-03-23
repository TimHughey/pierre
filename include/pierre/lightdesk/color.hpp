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

#include <cmath>
#include <ctgmath>

#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

class Color {
public:
  typedef double Val;

  typedef struct {
    double hue;
    double sat;
    double val;

    string_t asString() const;
  } Hsv;

  typedef struct {
    double l;
    double a;
    double b;

    string_t asString() const;
  } Lab;

  class Rgb {
  public:
    uint8_t r;
    uint8_t g;
    uint8_t b;

    Rgb() = default;
    Rgb(uint8_t red, uint8_t grn, uint8_t blu) : r{red}, g{grn}, b{blu} {};
    Rgb(uint val);

    string_t asString() const;
  };

  typedef uint8_t White;

public:
  Color() = default;
  ~Color() = default;

  Color(const uint rgb_val);
  Color(const Rgb &rgb);
  Color(const Hsv &hsv);
  Color(const Color &rhs) = default;

  double brightness() const { return _hsv.val * 100.0; }
  void copyRgbToByteArray(uint8_t *array) const;
  double deltaE(const Color &c2) const;

  Hsv hsv() { return _hsv; }
  Lab lab() { return _lab; }
  Rgb rgb() { return _rgb; }

  bool nearBlack() const;
  bool notBlack() const { return *this != Color(); }

  bool operator==(const Color &rhs) const;
  bool operator!=(const Color &rhs) const;

  void scale(float to_val);
  void setBrightness(const float);

  static void setScaleMinMax(const float min, const float max) {
    _scale_min = min;
    _scale_max = max;
  }

  // conversions
  static Rgb hsvToRgb(const Hsv &hsv);
  static Hsv rgbToHsv(const Rgb &rgb);
  static Lab rgbToLab(const Rgb &rgb);

  // useful static colors
  static Color full() {
    Color color(0xffffff);
    color._white = 255;

    return std::move(color);
  }
  static Color black() { return std::move(Color()); }
  static Color none() { return std::move(Color()); }

  string_t asString() const {
    std::array<char, 384> buf;

    auto len =
        snprintf(buf.data(), buf.size(), "%s %s %s", _rgb.asString().c_str(),
                 _lab.asString().c_str(), _hsv.asString().c_str());

    return std::move(string_t(buf.data(), len));
  }

private:
  typedef struct {
    double x;
    double y;
    double z;

    string_t asString() const;
  } Xyz;

  typedef Xyz Reference;

private:
  static Xyz rgbToXyz(const Rgb &rgb);
  static Lab xyzToLab(const Xyz &xyz);

private:
  Hsv _hsv = {.hue = 0, .sat = 0, .val = 0};
  Lab _lab = {.l = 0, .a = 0, .b = 0};
  Rgb _rgb = {.r = 0, .g = 0, .b = 0};
  White _white = 0;

  static Reference _ref; // initialized to D65/2° standard illuminant

  static float _scale_min;
  static float _scale_max;
};

} // namespace lightdesk
} // namespace pierre

#endif