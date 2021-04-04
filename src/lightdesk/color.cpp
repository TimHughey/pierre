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

#include <array>
#include <boost/format.hpp>
#include <cmath>
#include <ctgmath>
#include <fstream>
#include <iostream>
#include <vector>

#include "lightdesk/color.hpp"

using boost::format;
using namespace std;

namespace pierre {
namespace lightdesk {

using Hsl = Color::Hsl;
using Rgb = Color::Rgb;

Color::Color(const uint rgb_val) {
  _rgb = Rgb(rgb_val);
  _hsl = rgbToHsl(_rgb);
}

Color::Color(const Hsl &hsl) : _hsl(hsl) {
  if (_hsl.hue > 1.0f) {
    _hsl.hue /= 360.0f;
  }

  if (_hsl.sat > 1.0f) {
    _hsl.sat /= 100.0f;
  }

  if (_hsl.lum > 1.0f) {
    _hsl.lum /= 100.0f;
  }

  _rgb = hslToRgb(_hsl);
}

void Color::copyRgbToByteArray(uint8_t *array) const {
  array[0] = _rgb.r;
  array[1] = _rgb.g;
  array[2] = _rgb.b;
  array[3] = _white;
}

bool Color::isBlack() const {
  return (_rgb.r == 0) && (_rgb.g == 0) && (_rgb.b == 0);
}

Color Color::interpolate(Color a, Color b, float t) {
  // Hue interpolation
  float h;
  float d = b.hue() - a.hue();
  if (a.hue() > b.hue()) {
    // Swap (a.h, b.h)
    swap(a.hue(), b.hue());

    d *= -1.0;
    t = 1.0 - t;
  }

  if (d > 0.5) // 180deg
  {
    a.hue() = a.hue() + 1;                              // 360deg
    h = fmod((a.hue() + t * (b.hue() - a.hue())), 1.0); // 360deg
  }
  if (d <= 0.5) // 180deg
  {
    h = a.hue() + t * d;
  }

  Color interpolated;

  interpolated.hue() = h;
  interpolated.sat() = a.sat() + t * (b.sat() - a.sat());
  interpolated.lum() = a.lum() + t * (b.lum() - a.lum());

  interpolated._rgb = hslToRgb(interpolated._hsl);

  return move(interpolated);
}

bool Color::operator==(const Color &rhs) const { return _rgb == rhs._rgb; }

bool Color::operator!=(const Color &rhs) const { return !(*this == rhs); }

Color &Color::rotateHue(const float step) {
  auto next_hue = (_hsl.hue * 360.0) + step;

  if ((next_hue > 360.0) || (next_hue < 0)) {
    next_hue = 360.0 - next_hue;
  }

  _hsl.hue = (next_hue / 360.0);

  _rgb = hslToRgb(_hsl);

  return *this;
}

Color &Color::setBrightness(float val) {
  auto x = val / 100.0;

  if ((unsigned)x <= 1.0) {
    _hsl.lum = x;

    _rgb = hslToRgb(_hsl);
  }

  return *this;
}

Color &Color::setBrightness(const Color &rhs) {
  setBrightness(rhs.brightness());

  return *this;
}

Color &Color::setBrightness(const MinMaxFloat &range, const float val) {
  MinMaxFloat brightness_range(0.0f, brightness());

  const auto x = range.interpolate(brightness_range, val);

  if (false) {
    static uint seq = 0;
    static ofstream log("/tmp/pierre/color.log", ios::trunc);

    if (x >= brightness()) {
      log << boost::format(
                 "%05u range(%0.2f,%0.2f) val(%0.2f) brightness(%0.1f) "
                 "=> %0.1f\n") %
                 seq++ % range.min() % range.max() % val % brightness() % x;

      log.flush();
    }
  }

  setBrightness(x);

  return *this;
}

Color &Color::setSaturation(float val) {
  auto x = val / 100.0;

  if ((unsigned)x <= 1.0) {
    _hsl.sat = x;

    _rgb = hslToRgb(_hsl);
  }

  return *this;
}

Color &Color::setSaturation(const Color &rhs) {
  setSaturation(rhs.saturation());

  return *this;
}

Color &Color::setSaturation(const MinMaxFloat &range, const float val) {
  MinMaxFloat saturation_range(0.0f, saturation());

  const auto x = range.interpolate(saturation_range, val);

  setSaturation(x);

  return *this;
}

Rgb Color::hslToRgb(const Hsl &hsl) {
  // H, S and L input range = 0 ÷ 1.0
  // R, G and B output range = 0 ÷ 255

  Rgb rgb;

  if (hsl.sat == 0) {

    rgb.r = (uint8_t)round(hsl.lum * 255.0);
    rgb.g = (uint8_t)round(hsl.lum * 255.0);
    rgb.b = (uint8_t)round(hsl.lum * 255.0);
  } else {
    double var_1, var_2;

    if (hsl.lum < 0.5) {
      var_2 = hsl.lum * (1.0 + hsl.sat);
    } else {
      var_2 = (hsl.lum + hsl.sat) - (hsl.sat * hsl.lum);
    }

    var_1 = 2.0 * hsl.lum - var_2;

    rgb.r = hueToRgb(var_1, var_2, hsl.hue + one_third);
    rgb.g = hueToRgb(var_1, var_2, hsl.hue);
    rgb.b = hueToRgb(var_1, var_2, hsl.hue - one_third);
  }

  return move(rgb);
}

uint8_t Color::hueToRgb(double v1, double v2, double vh) {
  if (vh < 0.0) {
    vh += 1.0;
  }

  if (vh > 1.0) {
    vh -= 1.0;
  }

  if ((6.0 * vh) < 1.0) {
    return (uint8_t)round((v1 + (v2 - v1) * 6.0 * vh) * 255.0);
  }

  if ((2.0 * vh) < 1.0) {
    return (uint8_t)round(v2 * 255.0);
  }
  if ((3.0 * vh) < 2.0) {
    return (uint8_t)round((v1 + (v2 - v1) * ((2.0 / 3.0) - vh) * 6.0) * 255.0);
  }

  return (uint8_t)round(v1 * 255.0);
}

Hsl Color::rgbToHsl(const Rgb &rgb) {

  // R, G and B input range = 0 ÷ 255
  // H, S and L output range = 0 ÷ 1.0

  Hsl hsl;

  auto red = (rgb.r / 255.0);
  auto grn = (rgb.g / 255.0);
  auto blu = (rgb.b / 255.0);

  auto var_Min = min(min(red, grn), blu); // Min. value of RGB
  auto var_Max = max(max(red, grn), blu); // Max. value of RGB
  auto del_Max = var_Max - var_Min;       // Delta RGB value

  hsl.lum = (var_Max + var_Min) / 2.0;

  if (del_Max == 0.0) // This is a gray, no chroma...
  {
    hsl.hue = 0.0;
    hsl.sat = 0.0;
  } else {
    // Chromatic data...
    if (hsl.lum < 0.5) {
      hsl.sat = del_Max / (var_Max + var_Min);
    } else {
      hsl.sat = del_Max / (2.0 - var_Max - var_Min);
    }

    double del_R = (((var_Max - red) / 6.0) + (del_Max / 2.0)) / del_Max;
    double del_G = (((var_Max - grn) / 6.0) + (del_Max / 2.0)) / del_Max;
    double del_B = (((var_Max - blu) / 6.0) + (del_Max / 2.0)) / del_Max;

    if (red == var_Max) {
      hsl.hue = del_B - del_G;
    } else if (grn == var_Max) {
      hsl.hue = (1.0 / 3.0) + del_R - del_B;
    } else if (blu == var_Max) {
      hsl.hue = (2.0 / 3.0) + del_G - del_R;
    }

    if (hsl.hue < 0) {
      hsl.hue += 1.0;
    }
    if (hsl.hue > 1) {
      hsl.hue -= 1.0;
    }
  }

  return move(hsl);
}

string Hsl::asString() const {
  array<char, 128> buf;

  auto hue_val = hue * 360.0;
  auto sat_val = sat * 100.0;
  auto lum_val = lum * 100.0;

  auto len = snprintf(buf.data(), buf.size(), "hsl(%7.2f,%5.1f,%5.1f)", hue_val,
                      sat_val, lum_val);

  return move(string(buf.begin(), len));
}

Rgb::Rgb(const uint val) {
  uint8_t *bytes = (uint8_t *)&val;

  r = bytes[2];
  g = bytes[1];
  b = bytes[0];
}

bool Rgb::operator==(const Rgb &rhs) const {
  return (r == rhs.r) && (g == rhs.g) && (b == rhs.b);
}

string Rgb::asString() const {
  array<char, 128> buf;

  auto len = snprintf(buf.data(), buf.size(), "rgb(0x%02x%02x%02x)", r, b, g);

  return move(string(buf.begin(), len));
}

} // namespace lightdesk
} // namespace pierre
