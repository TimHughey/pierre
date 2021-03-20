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

#include "audio/dsp.hpp"
#include "lightdesk/fx/fx.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class MajorPeak : public Fx {
public:
  struct Config {
    bool log = false;
  };

public:
  MajorPeak(std::shared_ptr<audio::Dsp> dsp);
  ~MajorPeak() = default;

  void begin() override {}
  void execute() override;
  const string_t &name() const override {
    static const string_t fx_name = "MajorPeak";

    return fx_name;
  }

private:
  struct FreqColor {
    struct {
      Freq_t low;
      Freq_t high;
    } freq;

    Color color;
  };

  typedef std::deque<FreqColor> Palette;

private:
  void handleLowFreq(const Peak &peak, const Color &color);
  void handleOtherFreq(const Peak &peak, const Color &color);
  Color lookupColor(const Peak &peak);
  void logPeak(const Peak &peak) const;
  void makePalette();
  void pushPaletteColor(Freq_t high, const Color &color);

private:
  Config _cfg;
  const float _mid_range_frequencies[13] = {349.2, 370.0, 392.0, 415.3, 440.0,
                                            466.2, 493.9, 523.2, 544.4, 587.3,
                                            622.2, 659.3, 698.5};

  std::shared_ptr<audio::Dsp> _dsp;

  spPinSpot main;
  spPinSpot fill;
  spLedForest led_forest;
  spElWire el_dance_floor;
  spElWire el_entry;

  static Palette _palette;

  struct {
    Peak main = Peak::zero();
    Peak fill = Peak::zero();
  } _last_peak;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
