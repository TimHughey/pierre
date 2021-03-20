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

#include "lightdesk/fx/colorbars.hpp"
#include "lightdesk/fx/majorpeak.hpp"
#include "lightdesk/lightdesk.hpp"

namespace pierre {
namespace lightdesk {

using namespace std;
using namespace chrono;
using namespace fx;

LightDesk::LightDesk(const Config &cfg, shared_ptr<audio::Dsp> dsp)
    : _cfg(cfg), _dsp(std::move(dsp)) {

  Fx::setTracker(_hunits);

  _hunits->insert<PinSpot>("main", 1);
  _hunits->insert<PinSpot>("fill", 7);
  _hunits->insert<DiscoBall>("discoball", 1);
  _hunits->insert<ElWire>("el dance", 2);
  _hunits->insert<ElWire>("el entry", 3);
  _hunits->insert<LedForest>("led forest", 4);

  main = _hunits->unit<PinSpot>("main");
  fill = _hunits->unit<PinSpot>("fill");
  led_forest = _hunits->unit<LedForest>("led forest");
  el_dance_floor = _hunits->unit<ElWire>("el dance");
  el_entry = _hunits->unit<ElWire>("el entry");
  discoball = _hunits->unit<DiscoBall>("discoball");

  const auto scale = Peak::scale();

  Color::setScaleMinMax(scale.min, scale.max);

  discoball->spin();

  // initial Fx is ColorBars
  _active_fx = make_unique<fx::ColorBars>();
}

LightDesk::~LightDesk() { Fx::resetTracker(); }

void LightDesk::executeFx() {
  _active_fx->execute();

  if (_active_fx->finished()) {

    if (_active_fx->matchName("ColorBars")) {
      _active_fx = make_unique<MajorPeak>(_dsp);
    }
  }
}

shared_ptr<thread> LightDesk::run() {
  auto t = make_shared<thread>([this]() { this->stream(); });

  return t;
}

void LightDesk::stream() {
  while (_shutdown == false) {
    this_thread::sleep_for(milliseconds(100));
    this_thread::yield();
  }
}

float Color::_scale_min = 50.0f;
float Color::_scale_max = 100.0f;

std::shared_ptr<HeadUnitTracker> fx::Fx::_tracker;

} // namespace lightdesk
} // namespace pierre
