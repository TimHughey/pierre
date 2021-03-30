/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
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

#include <chrono>
#include <math.h>

#include "core/state.hpp"
#include "lightdesk/fx/colorbars.hpp"
#include "lightdesk/fx/leave.hpp"
#include "lightdesk/fx/majorpeak.hpp"
#include "lightdesk/lightdesk.hpp"

namespace pierre {
namespace lightdesk {

using namespace std;
using namespace chrono;
using namespace fx;

LightDesk::LightDesk(const Config &cfg, shared_ptr<audio::Dsp> dsp)
    : _cfg(cfg), _dsp(std::move(dsp)) {

  Fx::setTracker(_tracker);

  _tracker->insert<PinSpot>("main", 1);
  _tracker->insert<PinSpot>("fill", 7);
  _tracker->insert<DiscoBall>("discoball", 1);
  _tracker->insert<ElWire>("el dance", 2);
  _tracker->insert<ElWire>("el entry", 3);
  _tracker->insert<LedForest>("led forest", 4);

  main = _tracker->unit<PinSpot>("main");
  fill = _tracker->unit<PinSpot>("fill");
  led_forest = _tracker->unit<LedForest>("led forest");
  el_dance_floor = _tracker->unit<ElWire>("el dance");
  el_entry = _tracker->unit<ElWire>("el entry");
  discoball = _tracker->unit<DiscoBall>("discoball");

  auto scale = audio::Peak::activeScale();

  lightdesk::Color::setScaleMinMax(scale);

  discoball->spin();

  // select the first Fx
  if (_cfg.colorbars.enable) {
    _active.fx = make_unique<fx::ColorBars>();
  } else {
    _active.fx = make_unique<fx::MajorPeak>();
  }
}

LightDesk::~LightDesk() { Fx::resetTracker(); }

void LightDesk::executeFx() {

  audio::spPeaks peaks = _dsp->peaks();

  lock_guard lck(_active.mtx);
  _active.fx->execute(peaks);

  if (_active.fx->finished()) {
    if (_active.fx->matchName("ColorBars")) {
      _active.fx = make_unique<MajorPeak>();
    }
  }
}

shared_ptr<thread> LightDesk::run() {
  auto t = make_shared<thread>([this]() { this->stream(); });

  return t;
}

void LightDesk::leave() {
  auto x = core::State::leavingDuration<seconds>().count();

  {
    lock_guard lck(_active.mtx);
    _active.fx = make_unique<Leave>();
  }

  cout << "leaving for " << x << " second";
  if (x > 1) {
    cout << "s";
  }

  cout << " (or Ctrl-C to quit immediately)";
  cout.flush();

  this_thread::sleep_for(core::State::leavingDuration<milliseconds>());

  core::State::shutdown();

  cout << endl;
}

void LightDesk::stream() {
  while (core::State::running()) {
    this_thread::sleep_for(milliseconds(100));
    this_thread::yield();
  }
}

std::shared_ptr<HeadUnitTracker> fx::Fx::_tracker;

} // namespace lightdesk
} // namespace pierre
