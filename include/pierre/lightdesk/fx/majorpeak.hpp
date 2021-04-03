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

#include <deque>

#include "lightdesk/fx/fx.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class MajorPeak : public Fx {
public:
  using Freq = audio::Freq;
  using Peak = audio::Peak;
  using Peaks = audio::spPeaks;
  using Color = lightdesk::Color;

public:
  struct Config {
    bool log = false;
  };

public:
  MajorPeak();
  ~MajorPeak() = default;

  void execute(Peaks peaks) override;
  const string &name() const override {
    static const string fx_name = "MajorPeak";

    return fx_name;
  }

private:
  typedef std::deque<Color> ReferenceColors;

private:
  void handleElWire(Peaks peaks);
  void handleLedForest(Peaks peaks);
  void handleLowFreq(const Peak &peak, const Color &color);
  void handleOtherFreq(const Peak &peak, const Color &color);
  void logPeak(const Peak &peak) const;
  static const Color makeColor(Color ref, const Peak &freq);
  void makeRefColors();
  Color &refColor(size_t index) const;

private:
  Config _cfg;

  spPinSpot main;
  spPinSpot fill;
  spLedForest led_forest;
  spElWire el_dance_floor;
  spElWire el_entry;

  static ReferenceColors _ref_colors;

  struct {
    Peak main = Peak::zero();
    Peak fill = Peak::zero();
  } _last_peak;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
