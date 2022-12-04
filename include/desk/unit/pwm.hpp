//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/input_info.hpp"
#include "base/min_max_pair.hpp"
#include "base/types.hpp"
#include "desk/unit.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fmt/format.h>
#include <memory>
#include <ranges>

namespace pierre {

using DutyVal = uint32_t;
using DutyPercent = float;

class PulseWidth : public Unit {

private:
  enum mode_t : uint8_t { FIXED = 0, PULSE_INIT, PULSE_RUNNING };

public:
  PulseWidth(const auto &opts) : Unit(opts, unit::no_frame) {
    config.min = 0;
    config.max = 8190;
    config.dim = dutyPercent(0.004);
    config.bright = config.max;
    config.pulse_start = dutyPercent(0.5);
    config.pulse_end = dutyPercent(0.25);

    fixed(config.dim);
  }

  DutyVal duty() const { return _duty; }
  DutyVal dutyPercent(DutyPercent percent) const { return config.max * percent; }

  virtual void activate() noexcept override { fixed(config.bright); }
  virtual bool isBusy() const { return _mode == FIXED; }

  template <typename T = double> const min_max_pair<T> minMaxDuty() {
    return min_max_pair<T>(config.min, config.max);
  }

  void stop() { fixed(config.min); }

public:
  virtual void bright() { fixed(config.bright); }
  virtual void dark() noexcept override { fixed(config.min); }
  virtual void dim() { fixed(config.dim); }

  virtual void fixed(const DutyVal val) {
    duty_next(val);
    _mode = FIXED;
  }

  virtual void percent(const DutyPercent x) { fixed(duty_percent(x)); }

  virtual void prepare() noexcept override {
    const auto duty_now = duty();

    switch (_mode) {
    case FIXED:
      break;

    case PULSE_INIT:
      // duty_next() has already been set by the call to pulse()
      _mode = PULSE_RUNNING;
      break;

    case PULSE_RUNNING:
      const DutyVal fuzzy = _dest + _velocity;
      const DutyVal next = duty_now - _velocity;

      // we've reached or are close enough to the destination
      if ((duty_now <= fuzzy) || (next <= _dest)) {
        duty_next(_dest);
        _mode = FIXED;
      } else {
        duty_next(next);
      }

      break;
    }
  }

  virtual void update_msg(desk::DataMsg &msg) noexcept override {
    _duty = _duty_next;

    msg.doc[name] = _duty;
  }

  void pulse(float intensity = 1.0, float secs = 0.2) {
    // intensity is the percentage of max brightness for the pulse
    const float start = config.pulse_start * intensity;

    duty_next(start);
    _dest = config.pulse_end;

    // compute change per frame required to reach dark within requested secs
    _velocity = (start - _dest) / (InputInfo::fps * secs);

    _mode = PULSE_INIT;
  }

protected:
  struct {
    DutyVal min;
    DutyVal max;
    DutyVal dim;
    DutyVal bright;
    DutyVal pulse_start;
    DutyVal pulse_end;
  } config;

protected:
  virtual void duty_next(DutyVal duty) noexcept {
    _duty_next = std::ranges::clamp(duty, config.min, config.max);
  }

  virtual DutyPercent duty_percent(float x) { return x * config.max; }

private:
  mode_t _mode{FIXED};
  DutyVal _duty = 0;
  DutyVal _duty_next = 0;

  DutyVal _dest = 0;     // destination duty when traveling
  float _velocity = 0.0; // change per frame when fx is active
};

} // namespace pierre
