/*
    lightdesk/fx/majorpeak.hpp -- LightDesk Effect Major Peak
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

#pragma once

#include <boost/circular_buffer.hpp>

#include <deque>
#include <random>
#include <string>

#include "lightdesk/fx/fx.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class MajorPeak : public Fx {
public:
  using string = std::string;
  using ofstream = std::ofstream;
  using random_engine = std::mt19937;

  using Freq = audio::Freq;
  using Peak = audio::Peak;
  using Peaks = audio::spPeaks;
  using Color = lightdesk::Color;

  using circular_buffer = boost::circular_buffer<Peak>;

public:
  struct FloorCeiling {
    float floor;
    float ceiling;
  };

  struct Frequencies {
    struct FloorCeiling hard {
      .floor = 40.0, .ceiling = 10000.0
    };
    struct FloorCeiling soft {
      .floor = 110.0, .ceiling = 1500.0
    };
  };

  struct ColorRef {
    float hue;
    float sat;
    float bri;
  };

  struct ColorRotate {
    bool enable;
    uint32_t ms;
  };

  struct ColorConfig {
    bool random_start = false;

    struct ColorRef reference {
      .hue = 0.0, .sat = 100.0, .bri = 100
    };

    struct ColorRotate rotate {
      .enable = false, .ms = 7000
    };
  };

  struct ColorControl {
    struct {
      float min;
      float max;
      float step;
    } hue;

    struct {
      float max;
      bool mag_scaled;
    } brightness;
  };

  struct MakeColor {
    struct ColorControl above_soft_ceiling {
      .hue = {.min = 345.0, .max = 355.0, .step = 0.0001}, .brightness = {.max = 50.0, .mag_scaled = true }
    };

    struct ColorControl generic {
      .hue = {.min = 30.0, .max = 360.0, .step = 0.0001}, .brightness = {.max = 100.0, .mag_scaled = true }
    };
  };

  struct WhenGreater {
    float frequency;
    float brightness_min;
    struct {
      float brightness_min;
    } higher_frequency;
  };

  struct WhenLessThan {
    float frequency;
    float brightness_min;
  };

  struct FillPinspot {
    string name{"fill"};
    uint32_t fade_max_ms = 800;
    float frequency_max = 1000.0;

    struct WhenGreater when_greater {
      .frequency = 180.0, .brightness_min = 3.0, .higher_frequency = {.brightness_min = 80.0 }
    };

    struct WhenLessThan when_lessthan {
      .frequency = 180.0, .brightness_min = 27.0
    };
  };

  struct WhenFading {
    float brightness_min;
    struct {
      float brightness_min;
    } frequency_greater;
  };

  struct MainPinspot {
    string name{"main"};
    uint32_t fade_max_ms = 700;
    float frequency_min = 180.0;

    struct WhenFading when_fading {
      .brightness_min = 5.0, .frequency_greater = {.brightness_min = 69.0 }
    };

    struct WhenLessThan when_lessthan {
      .frequency = 180.0, .brightness_min = 27.0
    };
  };

public:
  MajorPeak();
  ~MajorPeak() = default;

  void executeFx(Peaks peaks) override;
  const string &name() const override {
    static const string fx_name = "MajorPeak";

    return fx_name;
  }

  void once() override;

private:
  typedef std::deque<Color> ReferenceColors;

private:
  void handleElWire(Peaks peaks);
  void handleLedForest(Peaks peaks);
  void handleFillPinspot(const Peaks &peaks);
  void handleMainPinspot(const Peaks peaks);

  const Color makeColor(Color ref, const Peak &freq);
  void makeRefColors();
  double randomHue();
  float randomRotation();
  Color &refColor(size_t index) const;

  bool useablePeak(const Peak &peak);

private:
  ColorConfig _color_config;
  Frequencies _freq;
  MakeColor _makecolor;

  MainPinspot _main_spot_cfg;
  FillPinspot _fill_spot_cfg;

  spPinSpot main;
  spPinSpot fill;
  spLedForest led_forest;
  spElWire el_dance_floor;
  spElWire el_entry;

  random_engine _random;

  Color _color;

  static ReferenceColors _ref_colors;

  struct {
    Peak main = Peak::zero();
    Peak fill = Peak::zero();
  } _last_peak;

  circular_buffer _prev_peaks;

  circular_buffer _main_history;
  circular_buffer _fill_history;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre