/*
    Pierre - Custom Light Show for Wiss Landing
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

#ifndef pierre_lightdesk_hpp
#define pierre_lightdesk_hpp

#include <mutex>
#include <set>
#include <thread>

#include "audio/dsp.hpp"
#include "dmx/producer.hpp"
#include "lightdesk/headunits/discoball.hpp"
#include "lightdesk/headunits/elwire.hpp"
#include "lightdesk/headunits/ledforest.hpp"
#include "lightdesk/headunits/pinspot/color.hpp"
#include "lightdesk/headunits/pinspot/pinspot.hpp"
#include "lightdesk/headunits/tracker.hpp"
#include "misc/mqx.hpp"

namespace pierre {
namespace lightdesk {

class LightDesk : public dmx::Producer {

public:
  struct Config {
    struct {
      float dB_floor = 44.2f;
      bool log = false;
    } majorpeak;

    struct {
      struct {
        float max = 65.0f;
        float min = 44.2f;
      } scale;
    } color;
  };

  struct FreqColor {
    struct {
      Freq_t low;
      Freq_t high;
    } freq;

    Color_t color;
  };

  typedef std::deque<FreqColor> Palette;

public:
  LightDesk(const Config &cfg, std::shared_ptr<audio::Dsp> dsp);
  ~LightDesk() = default;

  LightDesk(const LightDesk &) = delete;
  LightDesk &operator=(const LightDesk &) = delete;

  void prepare() override;

  std::shared_ptr<std::thread> run();

  void update(dmx::UpdateInfo &info) override;

private:
  void executeFx();
  void handleLowFreq(const Peak &peak, const Color_t &color);
  void handleOtherFreq(const Peak &peak, const Color_t &color);
  void logPeak(const Peak &peak) const;
  Color_t lookupColor(const Peak &peak);
  void makePalette();
  void pushPaletteColor(Freq_t high, const Color_t &color);
  void stream();

private:
  typedef std::shared_ptr<HeadUnitTracker> HUnits;

private:
  int _init_rc = 1;
  bool _shutdown = false;

  Config _cfg;

  std::shared_ptr<audio::Dsp> _dsp;

  HUnits _hunits = std::make_shared<HeadUnitTracker>();

  spPinSpot main;
  spPinSpot fill;
  spLedForest led_forest;
  spElWire el_dance_floor;
  spElWire el_entry;
  spDiscoBall discoball;

  static Palette _palette;

  struct {
    Peak main = Peak::zero();
    Peak fill = Peak::zero();
  } _last_peak;
};

} // namespace lightdesk
} // namespace pierre

#endif
