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
#include <map>
#include <mutex>

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
  MajorPeak();
  ~MajorPeak() = default;

  void dumpHistogram() const;

  void execute(audio::spPeaks peaks) override;
  const string_t &name() const override {
    static const string_t fx_name = "MajorPeak";

    return fx_name;
  }

private:
  struct FreqColor {

    struct {
      audio::Freq_t low;
      audio::Freq_t high;
    } freq;

    lightdesk::Color color;

    static const FreqColor zero() {
      FreqColor x;
      x.freq = {.low = 0, .high = 0};
      x.color = lightdesk::Color::black();

      return std::move(x);
    }
  };

  typedef std::deque<FreqColor> Palette;

private:
  void handleElWire(audio::spPeaks peaks);
  void handleLedForest(audio::spPeaks peaks);
  void handleLowFreq(const audio::Peak &peak, const lightdesk::Color &color);
  void handleOtherFreq(const audio::Peak &peak, const lightdesk::Color &color);
  void histogramInit();
  lightdesk::Color lookupColor(const audio::Peak &peak);
  void logPeak(const audio::Peak &peak) const;
  void makePalette();
  void makePaletteDefault();
  void pushPaletteColor(audio::Freq_t high, const lightdesk::Color &color);
  void recordColor(const FreqColor &freq_color);

private:
  Config _cfg;
  const float _mid_range_frequencies[13] = {349.2, 370.0, 392.0, 415.3, 440.0,
                                            466.2, 493.9, 523.2, 544.4, 587.3,
                                            622.2, 659.3, 698.5};

  spPinSpot main;
  spPinSpot fill;
  spLedForest led_forest;
  spElWire el_dance_floor;
  spElWire el_entry;

  static Palette _palette;

  struct {
    audio::Peak main = audio::Peak::zero();
    audio::Peak fill = audio::Peak::zero();
  } _last_peak;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
