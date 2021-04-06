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

#include <algorithm>
#include <boost/format.hpp>
#include <cmath>
#include <ctgmath>
#include <fstream>
#include <iostream>

#include "lightdesk/color.hpp"

using boost::format;
using namespace std;

namespace pierre {
namespace lightdesk {

Hsb::Hsb(const float hue, const float sat, const float bri) {
  if ((hue >= 0.0f) && (hue <= 360.0f)) {
    _hue = hue / 360.0f;
  }

  if ((sat >= 0.0f) && (sat <= 100.0f)) {
    _sat = sat / 100.0f;
  }

  if ((bri >= 0.0f) && (bri <= 100.0f)) {
    _bri = bri / 100.0f;
  }
}

bool Hsb::operator==(const Hsb &rhs) const {
  // test brightness first since it is most often adjusted
  return (_bri == rhs._bri) && (_hue == rhs._hue) && (_sat == rhs._sat);
}

Hsb Hsb::fromRgb(uint32_t rgb_val) {
  uint8_t *bytes = (uint8_t *)&rgb_val;

  return std::move(fromRgb(bytes[2], bytes[1], bytes[0]));
}

Hsb Hsb::fromRgb(uint8_t red_val, uint8_t grn_val, uint8_t blu_val) {
  using namespace std;

  float red = static_cast<float>(red_val) / 255.0f;
  float grn = static_cast<float>(grn_val) / 255.0f;
  float blu = static_cast<float>(blu_val) / 255.0f;

  Hsb hsb;

  auto &hue = hsb._hue;
  float &sat = hsb._sat;
  float &bri = hsb._bri;

  float chroma_max = max(max(red, grn), blu);
  float chroma_min = min(min(red, grn), blu);
  float chroma_delta = chroma_max - chroma_min;

  if (chroma_delta > 0) {
    if (chroma_max == red) {
      hue = 60.0f * (fmod(((grn - blu) / chroma_delta), 6.0f));
    } else if (chroma_max == grn) {
      hue = 60.0f * (((blu - red) / chroma_delta) + 2.0f);
    } else if (chroma_max == blu) {
      hue = 60.0f * (((red - grn) / chroma_delta) + 4.0f);
    }

    if (chroma_max > 0) {
      sat = chroma_delta / chroma_max;
    }

    bri = chroma_max;
  } else {
    bri = chroma_max;
  }

  if (hue < 0) {
    hue += 360.0f;
  }

  return move(hsb);
}

void Hsb::toRgb(uint8_t &red_val, uint8_t &grn_val, uint8_t &blu_val) const {
  float chroma = _bri * _sat;
  float hue_prime = fmod((360.0f * _hue) / 60.0f, 6.0f);
  float x = chroma * (1.0f - fabs(fmod(hue_prime, 2.0f) - 1.0f));
  float m = _bri - chroma;

  float red, grn, blu;

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

  red_val = static_cast<uint8_t>(round((red + m) * 255.0f));
  grn_val = static_cast<uint8_t>(round((grn + m) * 255.0f));
  blu_val = static_cast<uint8_t>(round((blu + m) * 255.0f));
}

Color::Color(const uint rgb_val) { _hsb = std::move(Hsb::fromRgb(rgb_val)); }

Color::Color(const Hsb &hsb) : _hsb(hsb) {}

void Color::copyRgbToByteArray(uint8_t *array) const {
  _hsb.toRgb(array[0], array[1], array[2]);
  array[3] = _white;
}

bool Color::isBlack() const { return (_hsb.saturation() == 0); }

bool Color::isWhite() const {
  return ((_hsb.brightness() == 100.0f) && (_hsb.saturation() == 0.0f)) ||
         (_white > 0);
}

Color Color::interpolate(Color a, Color b, float t) {
  auto &a_hsb = a._hsb;
  auto &b_hsb = a._hsb;

  // Hue interpolation
  float h;
  float d = b_hsb.hue() - a_hsb.hue();
  if (a_hsb.hue() > b_hsb.hue()) {
    // Swap (a.h, b.h)
    swap(a_hsb._hue, b_hsb._hue);

    d *= -1.0;
    t = 1.0 - t;
  }

  if (d > 0.5) // 180deg
  {
    a_hsb._hue = a_hsb.hue() + 1; // 360deg
    h = fmod((a_hsb.hue() + t * (b_hsb.hue() - a_hsb.hue())),
             1.0); // 360deg
  }
  if (d <= 0.5) // 180deg
  {
    h = a_hsb.hue() + t * d;
  }

  Color ipol;

  ipol._hsb._hue = h;
  ipol._hsb._sat = a.saturation() + t * (b.saturation() - a.saturation());
  ipol._hsb._bri = a.brightness() + t * (b.brightness() - a.brightness());

  return move(ipol);
}

bool Color::operator==(const Color &rhs) const { return _hsb == rhs._hsb; }

bool Color::operator!=(const Color &rhs) const { return !(*this == rhs); }

Color &Color::rotateHue(const float step) {
  auto next_hue = hue() + step;

  setHue(next_hue);

  return *this;
}

Color &Color::setBrightness(float val) {
  auto x = val / 100.0;

  _hsb._bri = x;

  return *this;
}

Color &Color::setBrightness(const Color &rhs) {
  setBrightness(rhs.brightness());

  return *this;
}

Color &Color::setHue(float hue) {
  if ((hue > 360.0f) || (hue < 0.0f)) {
    hue -= 360.0f;
  }

  _hsb._hue = hue / 360.0f;

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

  _hsb._sat = x;

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

string Color::asString() const {
  using namespace boost;

  uint8_t red, grn, blu = 0;

  _hsb.toRgb(red, grn, blu);

  format fmt("hsb(%7.2f, %5.1f, %5.1f) rgb(%4u, %4u, %4u)");

  fmt % hue() % saturation() % brightness() % (uint)red % (uint)grn % (uint)blu;

  return std::move(fmt.str());
}

} // namespace lightdesk
} // namespace pierre
