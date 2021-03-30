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
#include <fstream>
#include <iostream>
#include <vector>

#include <cmath>
#include <ctgmath>

#include "lightdesk/color.hpp"

using namespace std;

namespace pierre {
namespace lightdesk {

Color::Color(const uint rgb_val) {
  const Rgb rgb(rgb_val);

  *this = std::move(Color(rgb));
}

Color::Color(const Rgb &rgb) : _rgb(rgb) {
  _hsl = rgbToHsl(_rgb);
  // _hsv = rgbToHsv(_rgb);
  // _lab = rgbToLab(_rgb);
}

// Color::Color(const Hsv &hsv) : _hsv(hsv) {
//   _rgb = hsvToRgb(_hsv);
//   _lab = rgbToLab(_rgb);
// }

void Color::copyRgbToByteArray(uint8_t *array) const {
  array[0] = _rgb.r;
  array[1] = _rgb.g;
  array[2] = _rgb.b;
  array[3] = _white;
}

// double Color::deltaE(const Color &c2) const {
//   double de = sqrt(pow(_lab.l * 1.0 - c2._lab.l * 2.0, 2.0) +
//                    pow(_lab.a * 1.0 - c2._lab.a * 2.0, 2.0) +
//                    pow(_lab.a * 1.0 - c2._lab.b * 2.0, 2.0));
//
//   return de;
// }

bool Color::isBlack() const {
  return (_rgb.r == 0) && (_rgb.g == 0) && (_rgb.b == 0);
}

Color Color::interpolate(Color a, Color b, float t) {
  // Hue interpolation
  float h;
  float d = b.hue() - a.hue();
  if (a.hue() > b.hue()) {
    // Swap (a.h, b.h)
    std::swap(a.hue(), b.hue());

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
  // interpolated._hsv = rgbToHsv(interpolated._rgb);
  // interpolated._lab = rgbToLab(interpolated._rgb);

  return std::move(interpolated);
}

// bool Color::nearBlack() const {
//   auto rc = false;
//
//   if (deltaE(Color::black()) <= 3.0) {
//     rc = true;
//   }
//
//   return rc;
// }

bool Color::operator==(const Color &rhs) const { return _rgb == rhs._rgb; }

bool Color::operator!=(const Color &rhs) const { return !(*this == rhs); }

void Color::scale(const float val) {
  // static ofstream log("/tmp/pierre/color.log", std::ios::trunc);
  // static uint seq = 0;
  // Result := ((Input - InputLow) / (InputHigh - InputLow))
  //       * (OutputHigh - OutputLow) + OutputLow;

  auto smax = _scale->max();
  auto smin = _scale->min();

  const double new_brightness = ((val - smin) / (smax - smin)) * brightness();

  // if (new_brightness > brightness()) {
  // log << seq++ << " ";
  // log << "color scale brightness: min[" << smin << "] ";
  // log << "max[" << smax << "] ";
  // log << "val[" << val << "] ";
  // log << "initial= " << brightness();
  // log << " scaled=" << new_brightness << endl;
  // }

  setBrightness(new_brightness);
}

void Color::setBrightness(float val) {
  auto x = val / 100.0;

  if ((unsigned)x <= 100.0) {

    _hsl.lum = (x);

    _rgb = hslToRgb(_hsl);
    // _hsv = rgbToHsv(_rgb);
    // _lab = rgbToLab(_rgb);}
  }
}

Color::Rgb Color::hslToRgb(const Hsl &hsl) {
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

  return std::move(rgb);
}

// Color::Rgb Color::hsvToRgb(const Hsv &hsv) {
//
//   struct RgbCalc {
//     double r;
//     double g;
//     double b;
//   };
//
//   // H, S and V input range = 0 ÷ 1.0
//   // R, G and B output range = 0 ÷ 255
//
//   Rgb rgb;
//
//   if (hsv.sat == 0) {
//     rgb.r = hsv.val * 255;
//     rgb.g = hsv.val * 255;
//     rgb.b = hsv.val * 255;
//   } else {
//     auto var_h = hsv.hue * 6;
//     if (var_h == 6) {
//       var_h = 0;
//     }                          // H must be < 1
//     auto var_i = floor(var_h); // Or ... var_i = floor( var_h )
//     auto var_1 = hsv.val * (1.0 - hsv.sat);
//     auto var_2 = hsv.val * (1.0 - hsv.sat * (var_h - var_i));
//     auto var_3 = hsv.val * (1.0 - hsv.sat * (1.0 - (var_h - var_i)));
//
//     RgbCalc calc;
//
//     if (var_i == 0) {
//       calc = RgbCalc{.r = hsv.val, .g = var_3, .b = var_1};
//     } else if (var_i == 1) {
//       calc = RgbCalc{.r = var_i, .g = hsv.val, .b = var_1};
//     } else if (var_i == 2) {
//       calc = RgbCalc{.r = var_1, .g = hsv.val, .b = var_3};
//     } else if (var_i == 3) {
//       calc = RgbCalc{.r = var_1, .g = var_2, .b = hsv.val};
//     } else if (var_i == 4) {
//       calc = RgbCalc{.r = var_3, .g = var_1, .b = hsv.val};
//     } else {
//       calc = RgbCalc{.r = hsv.val, .g = var_1, .b = var_2};
//     }
//
//     rgb = Rgb{.r = (uint8_t)floor(calc.r * 255),
//               .g = (uint8_t)floor(calc.g * 255),
//               .b = (uint8_t)floor(calc.b * 255)};
//   }
//
//   return std::move(rgb);
// }

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

// Color::Rgb Color::labToRgb(const Lab &lab) {
//   const Xyz xyz = labToXyz(lab);
//   const Rgb rgb = xyzToRgb(xyz);
//
//   return std::move(rgb);
// }

// Color::Xyz Color::labToXyz(const Lab &lab) {
//
//   auto var_Y = (lab.l + 16.0) / 116.0;
//   auto var_X = lab.a / 500.0 + var_Y;
//   auto var_Z = var_Y - lab.b / 200.0;
//
//   if (pow(var_Y, 3.0) > 0.008856) {
//     var_Y = pow(var_Y, 3.0);
//   } else {
//     var_Y = (var_Y - 16.0 / 116.0) / 7.787;
//   }
//   if (pow(var_X, 3.0) > 0.008856) {
//     var_X = pow(var_X, 3.0);
//   } else {
//     var_X = (var_X - 16.0 / 116.0) / 7.787;
//   }
//   if (pow(var_Z, 3.0) > 0.008856) {
//     var_Z = pow(var_Z, 3.0);
//   } else {
//     var_Z = (var_Z - 16.0 / 116.0) / 7.787;
//   }
//
//   Xyz xyz{.x = var_X * _ref.x, .y = var_Y * _ref.y, .z = var_Z * _ref.z};
//
//   return std::move(xyz);
// }

Color::Hsl Color::rgbToHsl(const Rgb &rgb) {

  // R, G and B input range = 0 ÷ 255
  // H, S and L output range = 0 ÷ 1.0

  Hsl hsl;

  auto red = (rgb.r / 255.0);
  auto grn = (rgb.g / 255.0);
  auto blu = (rgb.b / 255.0);

  auto var_Min = std::min(std::min(red, grn), blu); // Min. value of RGB
  auto var_Max = std::max(std::max(red, grn), blu); // Max. value of RGB
  auto del_Max = var_Max - var_Min;                 // Delta RGB value

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

  return std::move(hsl);
}

// Color::Hsv Color::rgbToHsv(const Rgb &rgb) {
//   // R, G and B input range = 0 ÷ 255
//   // H, S and V output range = 0 ÷ 1.0
//
//   Hsv hsv;
//
//   float red = (rgb.r / 255.0);
//   float grn = (rgb.g / 255.0);
//   float blu = (rgb.b / 255.0);
//
//   double var_Min = std::min(std::min(red, grn), blu); // Min. value of RGB
//   float var_Max = std::max(std::max(red, grn), blu);  // Max. value of RGB
//   float del_Max = var_Max - var_Min;                  // Delta RGB value
//
//   hsv.val = var_Max;
//
//   if (del_Max == 0) // This is a gray, no chroma...
//   {
//     hsv.hue = 0;
//     hsv.sat = 0;
//   } else // Chromatic data...
//   {
//     hsv.sat = del_Max / var_Max;
//
//     double del_R = (((var_Max - red) / 6.0) + (del_Max / 2.0)) / del_Max;
//     double del_G = (((var_Max - grn) / 6.0) + (del_Max / 2.0)) / del_Max;
//     double del_B = (((var_Max - blu) / 6.0) + (del_Max / 2.0)) / del_Max;
//
//     if (red == var_Max) {
//       hsv.hue = del_B - del_G;
//     } else if (grn == var_Max) {
//       hsv.hue = (1.0 / 3.0) + del_R - del_B;
//     } else if (blu == var_Max) {
//       hsv.hue = (2.0 / 3.0) + del_G - del_R;
//     }
//
//     if (hsv.hue < 0) {
//       hsv.hue += 1.0;
//     }
//     if (hsv.hue > 1) {
//       hsv.hue -= 1.0;
//     }
//   }
//
//   return std::move(hsv);
// }
//
// Color::Lab Color::rgbToLab(const Rgb &rgb) {
//   const Xyz xyz = rgbToXyz(rgb);
//   const Lab lab = xyzToLab(xyz);
//
//   return std::move(lab);
// }
//
// Color::Xyz Color::rgbToXyz(const Rgb &rgb) {
//   Xyz xyz;
//   // sR, sG and sB (Standard RGB) input range = 0 ÷ 255
//   // X, Y and Z output refer to a D65/2° standard illuminant.
//
//   double var_R = ((double)rgb.r / 255.0);
//   double var_G = ((double)rgb.g / 255.0);
//   double var_B = ((double)rgb.b / 255.0);
//
//   if (var_R > 0.04045) {
//     var_R = pow(((var_R + 0.055) / 1.055), 2.4);
//   } else {
//     var_R = var_R / 12.92;
//   }
//
//   if (var_G > 0.04045) {
//     var_G = pow(((var_G + 0.055) / 1.055), 2.4);
//   } else {
//     var_G = var_G / 12.92;
//   }
//   if (var_B > 0.04045) {
//     var_B = pow(((var_B + 0.055) / 1.055), 2.4);
//   } else {
//     var_B = var_B / 12.92;
//   }
//
//   var_R = var_R * 100.0;
//   var_G = var_G * 100.0;
//   var_B = var_B * 100.0;
//
//   xyz.x = var_R * 0.4124 + var_G * 0.3576 + var_B * 0.1805;
//   xyz.y = var_R * 0.2126 + var_G * 0.7152 + var_B * 0.0722;
//   xyz.z = var_R * 0.0193 + var_G * 0.1192 + var_B * 0.9505;
//
//   return std::move(xyz);
// }
//
// Color::Lab Color::xyzToLab(const Xyz &xyz) {
//   Lab lab;
//   constexpr double adjust = 16.0 / 116.0;
//
//   // Reference-X, Y and Z refer to specific illuminants and observers.
//   // using equal weight of 100
//   double var_X = xyz.x / _ref.x;
//   double var_Y = xyz.y / _ref.y;
//   double var_Z = xyz.z / _ref.z;
//
//   if (var_X > 0.008856) {
//     var_X = pow(var_X, one_third);
//   } else {
//     var_X = (7.787 * var_X) + adjust;
//   }
//   if (var_Y > 0.008856) {
//     var_Y = pow(var_Y, one_third);
//   } else {
//     var_Y = (7.787 * var_Y) + adjust;
//   }
//   if (var_Z > 0.008856) {
//     var_Z = pow(var_Z, one_third);
//   } else {
//     var_Z = (7.787 * var_Z) + adjust;
//   }
//
//   lab.l = (116.0 * var_Y) - 16.0;
//   lab.a = 500.0 * (var_X - var_Y);
//   lab.b = 200.0 * (var_Y - var_Z);
//
//   return std::move(lab);
// }
//
// Color::Rgb Color::xyzToRgb(const Xyz &xyz) {
//   // X, Y and Z input refer to a D65/2° standard illuminant.
//   // sR, sG and sB (standard RGB) output range = 0 ÷ 255
//
//   auto var_X = xyz.x / 100.0;
//   auto var_Y = xyz.y / 100.0;
//   auto var_Z = xyz.z / 100.0;
//
//   auto var_R = var_X * 3.2406 + var_Y * -1.5372 + var_Z * -0.4986;
//   auto var_G = var_X * -0.9689 + var_Y * 1.8758 + var_Z * 0.0415;
//   auto var_B = var_X * 0.0557 + var_Y * -0.2040 + var_Z * 1.0570;
//
//   constexpr double p = 1.0 / 2.4;
//
//   if (var_R > 0.0031308) {
//     var_R = 1.055 * pow(var_R, p) - 0.055;
//   } else {
//     var_R = 12.92 * var_R;
//   }
//   if (var_G > 0.0031308) {
//     var_G = 1.055 * pow(var_G, p) - 0.055;
//   } else {
//     var_G = 12.92 * var_G;
//   }
//   if (var_B > 0.0031308) {
//     var_B = 1.055 * pow(var_B, p) - 0.055;
//   } else {
//     var_B = 12.92 * var_B;
//   }
//
//   Rgb rgb{.r = (uint8_t)round(var_R * 255.0),
//           .g = (uint8_t)round(var_G * 255.0),
//           .b = (uint8_t)round(var_B * 255.0)};
//
//   return std::move(rgb);
// }

// string_t Color::Hsv::asString() const {
//   std::array<char, 128> buf;
//
//   auto len =
//       snprintf(buf.data(), buf.size(), "hsv(%7.4f,%7.4f,%7.4f)", hue, sat,
//       val);
//
//   return std::move(string_t(buf.begin(), len));
// }
//
// string_t Color::Lab::asString() const {
//   std::array<char, 128> buf;
//
//   auto len =
//       snprintf(buf.data(), buf.size(), "lab(%10.4f,%10.4f,%10.4f)", l, a, b);
//
//   return std::move(string_t(buf.begin(), len));
// }

string_t Color::Hsl::asString() const {
  std::array<char, 128> buf;

  auto len =
      snprintf(buf.data(), buf.size(), "hsl(%7.4f,%7.4f,%7.4f)", hue, sat, lum);

  return std::move(string_t(buf.begin(), len));
}

Color::Rgb::Rgb(const uint val) {
  uint8_t *bytes = (uint8_t *)&val;

  r = bytes[2];
  g = bytes[1];
  b = bytes[0];
}

bool Color::Rgb::operator==(const Rgb &rhs) const {
  return (r == rhs.r) && (g == rhs.g) && (b == rhs.b);
}

string_t Color::Rgb::asString() const {
  std::array<char, 128> buf;

  auto len = snprintf(buf.data(), buf.size(), "rgb(%02x%02x%02x)", r, b, g);

  return std::move(string_t(buf.begin(), len));
}

// string_t Color::Xyz::asString() const {
//   std::array<char, 128> buf;
//
//   auto len =
//       snprintf(buf.data(), buf.size(), "xyz(%10.4f,%10.4f,%10.4f)", x, y, z);
//
//   return std::move(string_t(buf.begin(), len));
// }

// initialize static class data
// D65/2° standard illuminant
// Color::Reference Color::_ref = {.x = 95.047, .y = 100.0, .z = 108.883};
// Color::Reference Color::_ref = {.x = 100.0, .y = 100.0, .z = 100.0};

// scaling min and max
// float Color::_scale_min = 50.0;
// float Color::_scale_max = 100.0;

std::shared_ptr<MinMaxFloat> Color::_scale = MinMaxFloat::defaults();

} // namespace lightdesk
} // namespace pierre
