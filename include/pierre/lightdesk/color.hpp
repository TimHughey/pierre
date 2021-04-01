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

#include "local/types.hpp"
#include "misc/minmax.hpp"

namespace pierre {
namespace lightdesk {

class Color {
public:
  typedef double Val;

  typedef struct {
    double hue;
    double sat;
    double lum;

    string_t asString() const;
  } Hsl;

  class Rgb {
  public:
    uint8_t r;
    uint8_t g;
    uint8_t b;

    Rgb() = default;
    Rgb(uint8_t red, uint8_t grn, uint8_t blu) : r{red}, g{grn}, b{blu} {};
    Rgb(uint val);

    bool operator==(const Rgb &rhs) const;

    string_t asString() const;
  };

  typedef uint8_t White;

public:
  Color() = default;
  ~Color() = default;

  Color(const uint rgb_val);
  Color(const Rgb &rgb);
  Color(const Color &rhs) = default;

  double brightness() const { return _hsl.lum * 100.0; }
  void copyRgbToByteArray(uint8_t *array) const;

  double &hue() { return _hsl.hue; }
  double &sat() { return _hsl.sat; }
  double &lum() { return _hsl.lum; }

  Hsl hsl() { return _hsl; }
  Rgb rgb() { return _rgb; }

  static Color interpolate(Color a, Color b, float t);
  bool isBlack() const;
  bool nearBlack() const;
  bool notBlack() const { return isBlack() == false; }

  bool operator==(const Color &rhs) const;
  bool operator!=(const Color &rhs) const;

  void setBrightness(float val);
  void setBrightness(const MinMaxFloat &range, const float val);

  static void setScaleMinMax(std::shared_ptr<MinMaxFloat> scale) {
    _scale = scale;
  }

  // conversions
  static Rgb hslToRgb(const Hsl &hsl);
  static Hsl rgbToHsl(const Rgb &rgb);

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

    auto len = snprintf(buf.data(), buf.size(), "%s %s",
                        _rgb.asString().c_str(), _hsl.asString().c_str());

    return std::move(string_t(buf.data(), len));
  }

private:
  static uint8_t hueToRgb(double v1, double v2, double vh);

private:
  Hsl _hsl = {.hue = 0, .sat = 0, .lum = 0};
  Rgb _rgb = {.r = 0, .g = 0, .b = 0};
  White _white = 0;

  static constexpr double one_third = (1.0 / 3.0);
  static std::shared_ptr<MinMaxFloat> _scale;
};

} // namespace lightdesk
} // namespace pierre

#endif
