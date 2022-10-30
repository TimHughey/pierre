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

#include "base/color.hpp"
#include "base/minmax.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <cmath>
#include <ctgmath>
#include <fmt/format.h>

namespace pierre {
bool Hsb::operator==(const Hsb &rhs) const {
  // test brightness first since it is most often adjusted
  return (bri == rhs.bri) && (hue == rhs.hue) && (sat == rhs.sat);
}

Hsb Hsb::fromRgb(uint32_t rgb_val) {
  uint8_t *bytes = (uint8_t *)&rgb_val;

  return std::move(fromRgb(bytes[2], bytes[1], bytes[0]));
}

Hsb Hsb::fromRgb(uint8_t red_val, uint8_t grn_val, uint8_t blu_val) {
  const auto red = red_val / 255.0;
  const auto grn = grn_val / 255.0;
  const auto blu = blu_val / 255.0;

  double chroma_max = std::max(std::max(red, grn), blu);
  double chroma_min = std::min(std::min(red, grn), blu);
  double chroma_delta = chroma_max - chroma_min;

  double hue = 0, sat = 0, bri = 0;

  if (chroma_delta > 0) {
    if (chroma_max == red)
      hue = 60.0f * (fmod(((grn - blu) / chroma_delta), 6.0f));
    else if (chroma_max == grn)
      hue = 60.0f * (((blu - red) / chroma_delta) + 2.0f);
    else if (chroma_max == blu)
      hue = 60.0f * (((red - grn) / chroma_delta) + 4.0f);

    if (chroma_max > 0) sat = chroma_delta / chroma_max;

    bri = chroma_max;
  } else
    bri = chroma_max;

  if (hue < 0) hue += 360.0f;

  return Hsb{.hue = hue, .sat = sat, .bri = bri};
}

void Hsb::toRgb(uint8_t &red_val, uint8_t &grn_val, uint8_t &blu_val) const {
  double chroma = bri * sat;
  double hue_prime = std::fmod((360.0f * hue) / 60.0f, 6.0f);
  double x = chroma * (1.0f - std::fabs(std::fmod(hue_prime, 2.0f) - 1.0f));
  double m = bri - chroma;

  double red, grn, blu;

  if ((0 <= hue_prime) && (hue_prime < 1)) {
    red = chroma;
    grn = x;
    blu = 0;
  } else if ((1 <= hue_prime) && (hue_prime < 2)) {
    red = x;
    grn = chroma;
    blu = 0;
  } else if ((2 <= hue_prime) && (hue_prime < 3)) {
    red = 0;
    grn = chroma;
    blu = x;
  } else if ((3 <= hue_prime) && (hue_prime < 4)) {
    red = 0;
    grn = x;
    blu = chroma;
  } else if ((4 <= hue_prime) && (hue_prime < 5)) {
    red = x;
    grn = 0;
    blu = chroma;
  } else if ((5 <= hue_prime) && (hue_prime < 6)) {
    red = chroma;
    grn = 0;
    blu = x;
  } else {
    red = 0;
    grn = 0;
    blu = 0;
  }

  red_val = static_cast<uint8_t>(std::round((red + m) * 255.0f));
  grn_val = static_cast<uint8_t>(std::round((grn + m) * 255.0f));
  blu_val = static_cast<uint8_t>(std::round((blu + m) * 255.0f));
}

Color::Color(const uint rgb_val) { _hsb = std::move(Hsb::fromRgb(rgb_val)); }

Color::Color(const Hsb &hsb) : _hsb(hsb) {
  auto &hue = _hsb.hue;
  auto &sat = _hsb.sat;
  auto &bri = _hsb.bri;

  if ((hue >= 0.0f) && (hue <= 360.0f)) {
    hue = hue / 360.0f;
  }

  if ((sat >= 0.0f) && (sat <= 100.0f)) {
    sat = sat / 100.0f;
  }

  if ((bri >= 0.0f) && (bri <= 100.0f)) {
    bri = bri / 100.0f;
  }
}

void Color::copyRgbToByteArray(uint8_t *array) const {
  _hsb.toRgb(array[0], array[1], array[2]);
  array[3] = _white;
}

bool Color::isBlack() const { return (_hsb.sat == 0); }

bool Color::isWhite() const {
  if (((_hsb.bri == 100.0f) && (_hsb.sat == 0.0f)) || (_white > 0)) {
    return true;
  }

  return false;
}

Color Color::interpolate(Color a, Color b, double t) {
  auto &a_hsb = a._hsb;
  auto &b_hsb = a._hsb;

  // Hue interpolation
  double h;
  double d = b_hsb.hue - a_hsb.hue;
  if (a_hsb.hue > b_hsb.hue) {
    // Swap (a.h, b.h)
    std::swap(a_hsb.hue, b_hsb.hue);

    d *= -1.0;
    t = 1.0 - t;
  }

  if (d > 0.5) // 180deg
  {
    a_hsb.hue = a_hsb.hue + 1; // 360deg
    h = std::fmod((a_hsb.hue + t * (b_hsb.hue - a_hsb.hue)),
                  1.0); // 360deg
  }
  if (d <= 0.5) // 180deg
  {
    h = a_hsb.hue + t * d;
  }

  Color ipol;

  ipol._hsb.hue = h;
  ipol._hsb.sat = a.saturation() + t * (b.saturation() - a.saturation());
  ipol._hsb.bri = a.brightness() + t * (b.brightness() - a.brightness());

  return ipol;
}

bool Color::operator==(const Color &rhs) const { return _hsb == rhs._hsb; }

bool Color::operator!=(const Color &rhs) const { return !(*this == rhs); }

Color &Color::rotateHue(const double step) {
  auto next_hue = hue() + step;

  setHue(next_hue);

  return *this;
}

Color &Color::setHue(double hue) {
  if ((hue > 360.0f) || (hue < 0.0f)) {
    hue -= 360.0f;
  }

  _hsb.hue = hue / 360.0f;

  return *this;
}

Color &Color::setBrightness(const min_max_dbl &range, const double val) {
  min_max_dbl brightness_range(0.0f, brightness());
  setBrightness(range.interpolate(brightness_range, val));

  return *this;
}

Color &Color::setSaturation(double val) {
  auto x = val / 100.0;

  _hsb.sat = x;

  return *this;
}

Color &Color::setSaturation(const Color &rhs) {
  setSaturation(rhs.saturation());

  return *this;
}

Color &Color::setSaturation(const min_max_dbl &range, const double val) {
  min_max_dbl saturation_range(0.0f, saturation());

  const auto x = range.interpolate(saturation_range, val);

  setSaturation(x);

  return *this;
}

string Color::asString() const {
  uint8_t red, grn, blu = 0;
  _hsb.toRgb(red, grn, blu);

  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "hsb({:7.2}, {:5.1}, {:5.1}) ", hue(), saturation(), brightness());
  fmt::format_to(w, "rgb({:4}, {:4}, {:4})", red, grn, blu);

  return msg;
}

} // namespace pierre
