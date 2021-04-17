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

#ifndef pierre_lightdesk_fx_majorpeak_hpp
#define pierre_lightdesk_fx_majorpeak_hpp

#include <boost/circular_buffer.hpp>

#include <deque>
#include <fstream>
#include <iostream>
#include <ostream>
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
  struct MajorPeakConfig {

    struct {
      struct {
        float ceiling = 10000.0f;
        float floor = 40.0f;
      } hard;

      struct {
        float ceiling = 1500.0f;
        float floor = 110.0f;
      } soft;
    } freq;
  };

public:
  MajorPeak();
  ~MajorPeak() = default;

  void executeFx(Peaks peaks) override;
  const string &name() const override {
    static const string fx_name = "MajorPeak";

    return fx_name;
  }

private:
  typedef std::deque<Color> ReferenceColors;

private:
  void handleElWire(Peaks peaks);
  void handleLedForest(Peaks peaks);
  void handleFillPinspot(const Peaks &peaks);
  void handleMainPinspot(const Peaks peaks);

  void logPeak(const Peak &peak);
  void logPeakColor(float low, float deg, const Peak &peak, const Color &color);

  const Color makeColor(Color ref, const Peak &freq);
  void makeRefColors();
  double randomHue();
  float randomRotation();
  Color &refColor(size_t index) const;

  bool useablePeak(const Peak &peak);

private:
  MajorPeakConfig _cfg;

  spPinSpot main;
  spPinSpot fill;
  spLedForest led_forest;
  spElWire el_dance_floor;
  spElWire el_entry;

  random_engine _random;

  Color _color;

  static ReferenceColors _ref_colors;
  ofstream _log;

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

#endif
