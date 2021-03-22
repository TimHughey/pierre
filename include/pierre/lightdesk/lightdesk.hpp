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

#include <chrono>
#include <mutex>
#include <set>
#include <thread>

#include "audio/dsp.hpp"
#include "dmx/producer.hpp"

#include "lightdesk/fx/fx.hpp"
#include "lightdesk/headunits/discoball.hpp"
#include "lightdesk/headunits/elwire.hpp"
#include "lightdesk/headunits/ledforest.hpp"
#include "lightdesk/headunits/pinspot/color.hpp"
#include "lightdesk/headunits/pinspot/pinspot.hpp"
#include "lightdesk/headunits/tracker.hpp"
#include "misc/mqx.hpp"

namespace pierre {
namespace lightdesk {

using namespace std::chrono;

class LightDesk : public dmx::Producer {

public:
  struct Config {
    struct {
      bool log = false;
    } majorpeak;
  };

public:
  LightDesk(const Config &cfg, std::shared_ptr<audio::Dsp> dsp);
  ~LightDesk();

  LightDesk(const LightDesk &) = delete;
  LightDesk &operator=(const LightDesk &) = delete;

  void leave();

  void prepare() override { _tracker->prepare(); }

  std::shared_ptr<std::thread> run();

  void update(dmx::Packet &packet) override {
    executeFx();
    packet.rootObj()["ACP"] = true; // AC power on
    _tracker->update(packet);
  }

private:
  void executeFx();
  void stream();

  template <typename T> std::shared_ptr<T> unit(const string_t name) {
    return _tracker->unit<T>(name);
  }

private:
  typedef std::shared_ptr<HeadUnitTracker> HUnits;

private:
  int _init_rc = 1;
  Config _cfg;

  std::shared_ptr<audio::Dsp> _dsp;
  HUnits _tracker = std::make_shared<HeadUnitTracker>();

  struct {
    std::mutex mtx;
    std::unique_ptr<fx::Fx> fx;
  } _active;

  spPinSpot main;
  spPinSpot fill;
  spLedForest led_forest;
  spElWire el_dance_floor;
  spElWire el_entry;
  spDiscoBall discoball;
};

} // namespace lightdesk
} // namespace pierre

#endif
