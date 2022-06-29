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

#pragma once

#include "base/minmax.hpp"
#include "base/mqx.hpp"
#include "base/typical.hpp"
#include "desk/fx/fx.hpp"
#include "desk/headunits/discoball.hpp"
#include "desk/headunits/elwire.hpp"
#include "desk/headunits/ledforest.hpp"
#include "desk/headunits/pinspot.hpp"
#include "desk/headunits/tracker.hpp"

#include <memory>
#include <set>

namespace pierre {
namespace desk {

class Desk;
typedef std::shared_ptr<Desk> shDesk;

class Desk : public std::enable_shared_from_this<Desk>, dmx::Producer {
public:
  using string = std::string;

public:
  Desk(std::shared_ptr<audio::Dsp> dsp);
  ~Desk();

  Desk(const Desk &) = delete;
  Desk &operator=(const Desk &) = delete;

  std::shared_ptr<fx::Fx> activeFx() const { return _active.fx; }
  static std::shared_ptr<Desk> desk() { return _instance; }

  void leave();

  void prepare() override { _tracker->prepare(); }

  std::shared_ptr<std::jthread> run();

  void saveInstance(std::shared_ptr<Desk> desk) { _instance = desk; }

  void update(packet::DMX &packet) override {
    executeFx();
    packet.rootObj()["ACP"] = true; // AC power on
    _tracker->update(packet);
  }

private:
  void executeFx();
  void stream();

  template <typename T> std::shared_ptr<T> unit(const string name) {
    return _tracker->unit<T>(name);
  }

private:
  typedef std::shared_ptr<HeadUnitTracker> HUnits;

private:
  int _init_rc = 1;

  HUnits _tracker = std::make_shared<HeadUnitTracker>();

  struct {
    std::mutex mtx;
    std::shared_ptr<fx::Fx> fx;
  } _active;

  spPinSpot main;
  spPinSpot fill;
  spLedForest led_forest;
  spElWire el_dance_floor;
  spElWire el_entry;
  spDiscoBall discoball;

  static std::shared_ptr<Desk> _instance;
};

} // namespace desk
} // namespace pierre
