/*
    lightdesk/headunits/pwm_base.hpp - Ruth LightDesk Head Unit Pwm Base
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
#include "lightdesk/headunit.hpp"

namespace pierre {
namespace lightdesk {

//
// IMPORTANT!
//
// This object is subject to race conditions when multiple tasks call:
//  1. effects (e.g. dark(), pulse())
//  2. framePrepare()
//
// As coded this object is safe for a second task to call frameUpate().
//

class PulseWidthHeadUnit : public HeadUnit {
public:
  typedef uint_fast16_t DutyVal;
  typedef float DutyPercent;

public:
  PulseWidthHeadUnit(uint8_t num) : HeadUnit(num, 0) {
    _id.fill(0x00);

    config.min = 0;
    config.max = 8190;
    config.dim = dutyPercent(0.004);
    config.bright = config.max;
    config.leave = config.max;
    config.pulse_start = dutyPercent(0.5);
    config.pulse_end = dutyPercent(0.25);

    fixed(config.dim);
  }

  virtual ~PulseWidthHeadUnit() { stop(); }

  DutyVal duty() const { return _duty; }
  DutyVal dutyPercent(DutyPercent percent) const { return config.max * percent; }

  virtual bool isBusy() const { return _mode == FIXED; }
  virtual void leave() override { fixed(config.leave); }

  const MinMaxPair<DutyVal> minMaxDuty() { return MinMaxPair(config.min, config.max); }

  template <typename T> const MinMaxPair<T> minMaxDuty() {
    return MinMaxPair<T>(config.min, config.max);
  }

  void stop() { fixed(config.min); }

public:
  virtual void bright() { fixed(config.bright); }
  virtual void dark() { fixed(config.min); }
  virtual void dim() { fixed(config.dim); }

  virtual void fixed(const DutyVal val) {
    unitNext(val);
    _mode = FIXED;
  }

  virtual void percent(const DutyPercent x) { fixed(unitPercent(x)); }

  virtual void framePrepare() override {
    const uint32_t duty_now = duty();

    switch (_mode) {
      case FIXED:
        break;

      case PULSE_INIT:
        // unitNext() has already been set by the call to pulse()
        _mode = PULSE_RUNNING;
        break;

      case PULSE_RUNNING:
        const DutyVal fuzzy = _dest + _velocity;
        const DutyVal next = duty_now - _velocity;

        // we've reached or are close enough to the destination
        if ((duty_now <= fuzzy) || (next <= _dest)) {
          unitNext(_dest);
          _mode = FIXED;
        } else {
          unitNext(next);
        }

        break;
    }
  }

  virtual void frameUpdate(packet::DMX &packet) override {
    _duty = _unit_next;

    auto root = packet.rootObj();

    if (_id[0] != 0x00) {
      root[_id.data()] = _duty;
    }
  }

  void pulse(float intensity = 1.0, float secs = 0.2) {
    // intensity is the percentage of max brightness for the pulse
    const float start = config.pulse_start * intensity;

    unitNext(start);
    _dest = config.pulse_end;

    // compute change per frame required to reach dark within requested secs
    _velocity = (start - _dest) / (fps() * secs);

    _mode = PULSE_INIT;
  }

protected:
  struct {
    DutyVal min;
    DutyVal max;
    DutyVal dim;
    DutyVal bright;
    DutyVal leave;
    DutyVal pulse_start;
    DutyVal pulse_end;
  } config;

  std::array<char, 6> _id;

protected:
  virtual void unitNext(DutyVal duty) {
    if (duty > config.max) {
      duty = config.max;
    } else if (duty < config.min) {
      duty = config.min;
    }

    _unit_next = duty;
  }

  virtual DutyPercent unitPercent(float x) { return x * config.max; }

private:
  typedef enum { FIXED = 0, PULSE_INIT, PULSE_RUNNING } PulseWidthHeadUnit_t;

private:
  PulseWidthHeadUnit_t _mode = FIXED;
  DutyVal _duty = 0;
  DutyVal _unit_next = 0;

  DutyVal _dest = 0;     // destination duty when traveling
  float _velocity = 0.0; // change per frame when fx is active
};

typedef PulseWidthHeadUnit::DutyVal DutyVal;

} // namespace lightdesk
} // namespace pierre
