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

#ifndef pierre_pinspot_color_hpp
#define pierre_pinspot_color_hpp

#include "math.h"

#include "dmx/types.hpp"
#include "lightdesk/types.hpp"
#include "local/types.hpp"
#include "misc/random.hpp"

namespace pierre {
namespace lightdesk {

class Color {
public:
  Color(){};

  Color(const int val) {
    const Rgbw_t v = static_cast<Rgbw_t>(val);
    rgbw(v);
  }

  Color(const Rgbw_t &val) { rgbw(val); }

  Color(uint8_t red, uint8_t grn, uint8_t blu, uint8_t wht) {
    rgbw(red, grn, blu, wht);
  }

  Color(uint8_t red, uint8_t grn, uint8_t blu) { rgbw(red, grn, blu, 0); }

  Color(int r, int g, int b, int w) {
    const uint8_t red = static_cast<uint8_t>(r);
    const uint8_t grn = static_cast<uint8_t>(g);
    const uint8_t blu = static_cast<uint8_t>(b);
    const uint8_t wht = static_cast<uint8_t>(w);
    rgbw(red, grn, blu, wht);
  }

  Color(const Color_t &rhs) = default;

  // specific colors
  static Color_t none() { return Color(0); }
  static Color_t black() { return Color(0); }
  static Color_t bright() { return Color(255, 255, 255, 255); }

  static Color_t crimson() { return Color(220, 10, 30); }
  static Color_t red() { return Color(255, 0, 0); }
  static Color_t salmon() { return Color(250, 128, 114); }
  static Color_t fireBrick() { return Color(178, 34, 34); }
  static Color_t gold() { return Color(255, 215, 0); }
  static Color_t yellow() { return Color(255, 255, 0); }
  static Color_t yellow25() { return Color(64, 64, 0); }
  static Color_t yellow50() { return Color(128, 128, 0); }
  static Color_t yellow75() { return Color(191, 191, 0); }
  static Color_t green() { return Color(0, 255, 0, 0); }
  static Color_t lawnGreen() { return Color(124, 252, 0); }
  static Color_t seaGreen() { return Color(46, 139, 87); }
  static Color_t lightGreen() { return Color(144, 238, 144); }
  static Color_t limeGreen() { return Color(50, 205, 50); }
  static Color_t forestGreen() { return Color(34, 139, 34); }
  static Color_t teal() { return Color(0, 128, 128); }
  static Color_t cyan() { return Color(0, 255, 255, 0); }
  static Color_t blue() { return Color(0, 0, 255, 0); }
  static Color_t powderBlue() { return Color(176, 224, 230); }
  static Color_t cadetBlue() { return Color(95, 158, 160); }
  static Color_t steelBlue() { return Color(70, 130, 180); }
  static Color_t deepSkyBlue() { return Color(0, 191, 255); }
  static Color_t dodgerBlue() { return Color(30, 144, 255); }
  static Color_t magenta() { return Color(255, 0, 255, 0); }
  static Color_t blueViolet() { return Color(138, 43, 226); }
  static Color_t darkViolet() { return Color(148, 0, 211); }
  static Color_t deepPink() { return Color(255, 20, 74); }
  static Color_t hotPink() { return Color(255, 105, 180); }
  static Color_t pink() { return Color(255, 192, 203); }

  static Color_t lightBlue() { return Color{0, 0, 255, 255}; }
  static Color_t lightRed() { return Color(255, 0, 0, 255); }
  static Color_t lightViolet() { return Color(255, 255, 0, 255); }
  static Color_t lightYellow() { return Color(255, 255, 0, 255); }

  void copyToByteArray(uint8_t *array) const {
    for (auto i = 0; i < endOfParts(); i++) {
      const float val = rint(colorPartConst(i));
      array[i] = (uint8_t)val;
    }
  }

  void copyTo(dmx::Frame &frame, uint start_pos) {
    auto snippet = frame.begin() + start_pos;

    for (auto i = 0; i < endOfParts(); i++) {
      const float val = rint(colorPartConst(i));
      snippet[i] = (uint8_t)val;
    }
  }

  inline float &colorPart(ColorPart_t part) {
    const uint32_t i = static_cast<uint32_t>(part);

    return _parts[i];
  }

  inline float colorPartConst(uint32_t part) const { return _parts[part]; }

  inline float colorPartConst(ColorPart_t part) const {
    const uint32_t i = static_cast<uint32_t>(part);

    return _parts[i];
  }

  void diff(const Color_t &c1, const Color_t &c2, bool (&directions)[4]) {
    for (auto i = 0; i < endOfParts(); i++) {
      ColorPart_t cp = static_cast<ColorPart_t>(i);

      float p1 = c1.colorPartConst(cp);
      float p2 = c2.colorPartConst(cp);

      colorPart(cp) = abs(p1 - p2);
      directions[i] = (p2 > p1) ? true : false;
    }
  }

  inline uint8_t endOfParts() const {
    return static_cast<uint8_t>(END_OF_PARTS);
  }

  bool notBlack() const {
    bool rc = false;

    for (auto k = 0; (k < endOfParts()) && (rc == false); k++) {
      if (_parts[k] > 0) {
        rc = true;
      }
    }

    return rc;
  }

  Color_t operator=(const Rgbw_t val) {
    rgbw(val);
    return *this;
  }

  bool operator==(const Color_t &rhs) const {
    bool rc = true;

    for (auto k = 0; k < endOfParts(); k++) {
      rc &= (_parts[k] == rhs._parts[k]);
    }

    return rc;
  }

  bool operator<=(const Color_t &rhs) const {
    bool rc = true;

    for (auto k = 0; k < endOfParts(); k++) {
      rc &= (_parts[k] <= rhs._parts[k]);
    }

    return rc;
  }

  bool operator>=(const Color_t &rhs) const {
    bool rc = true;

    for (auto k = 0; k < endOfParts(); k++) {
      rc &= (_parts[k] >= rhs._parts[k]);
    }

    return rc;
  }

  void print() const {
    printf("r[%03.2f] g[%03.2f] b[%03.2f] w[%03.2f]\n",
           colorPartConst(RED_PART), colorPartConst(GREEN_PART),
           colorPartConst(BLUE_PART), colorPartConst(WHITE_PART));
  }

  static const Color randomize() {
    Color c;

    const uint32_t end_of_parts = static_cast<uint32_t>(WHITE_PART);
    for (uint32_t i = 0; i < end_of_parts; i++) {
      const ColorPart_t part = static_cast<ColorPart_t>(i);

      switch (roll2D6()) {
      case 2:
      case 12:
        c.colorPart(part) = 0.0f;
        break;

      case 3:
      case 11:
        c.colorPart(part) = static_cast<float>(random(128));
        break;

      case 4:
      case 5:
        c.colorPart(part) = static_cast<float>(random(64));
        break;

      case 6:
      case 7:
      case 8:
        c.colorPart(part) = static_cast<float>(random(127) + random(128));
        break;

      case 9:
      case 10:
        c.colorPart(part) = static_cast<float>(random(32));
        break;

      default:
        c.colorPart(part) = 10;
        break;
      }
    }

    return c;
  }

  void rgbw(const Rgbw_t val) {
    rgbw((val >> 24), (val >> 16), (val >> 8), (uint8_t)val);
  }

  void rgbw(uint8_t red, uint8_t grn, uint8_t blu, uint8_t wht) {
    colorPart(RED_PART) = static_cast<float>(red);
    colorPart(GREEN_PART) = static_cast<float>(grn);
    colorPart(BLUE_PART) = static_cast<float>(blu);
    colorPart(WHITE_PART) = static_cast<float>(wht);
  }

  inline void scale(float tobe_val) {
    // Result := ((Input - InputLow) / (InputHigh - InputLow))
    //       * (OutputHigh - OutputLow) + OutputLow;

    for (auto k = 0; k < endOfParts(); k++) {
      const uint32_t asis_val = _parts[k];
      const float ranged =
          ((tobe_val - _scale_min) / (_scale_max - _scale_min)) * asis_val;

      const uint8_t adjusted = (ranged / 255.0) * asis_val;

      if (adjusted < asis_val) {
        _parts[k] = adjusted;
      }
    }
  }

  static void setScaleMinMax(const float min, const float max) {
    _scale_min = min;
    _scale_max = max;
  }

private:
  float _parts[4] = {};

  static float _scale_min;
  static float _scale_max;
};

class ColorVelocity {
private:
  typedef enum { vRED, vGREEN, vBLUE, vWHITE } Velocity_t;

public:
  ColorVelocity() {}

  void calculate(const Color_t &begin, const Color_t &end, float travel_secs) {
    const float travel_frames = travel_secs * 44.0;
    Color distance;

    distance.diff(begin, end, _directions);

    const uint32_t end_of_parts = static_cast<uint32_t>(END_OF_PARTS);
    for (uint32_t i = 0; i < end_of_parts; i++) {
      const ColorPart_t part = static_cast<ColorPart_t>(i);

      velocity(part) = distance.colorPartConst(part) / travel_frames;
    }
  }

  float direction(ColorPart_t part) const {
    const uint32_t i = static_cast<uint32_t>(part);
    bool dir = _directions[i];
    float val = (dir) ? 1.0 : -1.0;

    return val;
  }

  void moveColor(Color_t &color, const Color_t &dest, bool &more_travel) {
    const uint32_t end_of_parts = static_cast<uint32_t>(END_OF_PARTS);
    for (uint32_t i = 0; i < end_of_parts; i++) {
      const ColorPart_t part = static_cast<ColorPart_t>(i);

      movePart(part, color, dest, more_travel);
    }
  }

  inline float &velocity(ColorPart_t part) {
    const uint32_t i = static_cast<uint32_t>(part);
    return _velocity[i];
  }

private:
  void movePart(ColorPart_t part, Color_t &color, const Color_t &dest_color,
                bool &more_travel) {
    const float dest = dest_color.colorPartConst(part);
    const float dir = direction(part);
    float new_pos = color.colorPart(part) + velocityActual(part);

    // if we've reached our dest then the new color is the destination
    if (dir == 1.0) {
      if (new_pos > dest) {
        new_pos = dest;
      }
    } else {
      if (new_pos < dest) {
        new_pos = dest;
      }
    }

    if (new_pos != dest) {
      more_travel |= true;
    }

    color.colorPart(part) = new_pos;
  }

  inline float velocityActual(ColorPart_t part) {
    const uint32_t i = static_cast<uint32_t>(part);
    return (_velocity[i] * direction(part));
  }

private:
  bool _directions[4] = {};
  float _velocity[4] = {};
};

} // namespace lightdesk
} // namespace pierre

#endif
