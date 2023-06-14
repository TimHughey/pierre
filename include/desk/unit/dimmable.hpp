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

#include "base/bound.hpp"
#include "base/input_info.hpp"
#include "base/types.hpp"
#include "desk/unit.hpp"

#include <cstdint>
#include <ranges>

namespace pierre {

namespace desk {

using duty_val = uint32_t;
using duty_percent = double;
using bound_duty = bound<double>;

class Dimmable : public Unit {

private:
  enum run_mode : uint8_t { FIXED = 0, PULSE_INIT, PULSE_RUNNING };

public:
  template <typename T>
  Dimmable(T &&name, auto addr) noexcept
      : Unit(std::forward<T>(name), addr, no_frame), _mode{FIXED} {
    config.min = 0;
    config.max = 8190;
    config.dim = dutyPercent(0.004);
    config.bright = config.max;
    config.pulse_start = dutyPercent(0.5);
    config.pulse_end = dutyPercent(0.25);

    fixed(config.dim);
  }

  duty_val duty() const { return _duty; }
  duty_val dutyPercent(duty_percent percent) const { return config.max * percent; }

  virtual void activate() noexcept override { fixed(config.bright); }
  virtual bool isBusy() const { return _mode == FIXED; }

  auto minMaxDuty() { return bound_duty({(double)config.min, (double)config.max}); }

  void stop() { fixed(config.min); }

public:
  virtual void bright() { fixed(config.bright); }
  virtual void dark() noexcept override { fixed(config.min); }
  virtual void dim() { fixed(config.dim); }

  virtual void fixed(const duty_val d) {
    duty_next(d);
    _mode = FIXED;
  }

  virtual void percent(const duty_percent x) { fixed(x * config.max); }

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
      const duty_val fuzzy = _dest + _velocity;
      const duty_val next = duty_now - _velocity;

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

  virtual void update_msg(DataMsg &msg) noexcept override {
    msg.add_kv(name, std::exchange(_duty, _duty_next));
  }

  void pulse(float intensity = 1.0, float secs = 0.2) {
    // intensity is the percentage of max brightness for the pulse
    float start = config.pulse_start * intensity;

    duty_next(start);
    _dest = config.pulse_end;

    // compute change per frame required to reach dark within requested secs
    _velocity = (start - _dest) / (InputInfo::fps * secs);

    _mode = PULSE_INIT;
  }

public:
  struct {
    duty_val min;
    duty_val max;
    duty_val dim;
    duty_val bright;
    duty_val pulse_start;
    duty_val pulse_end;
  } config;

protected:
  virtual void duty_next(duty_val d) noexcept {
    _duty_next = std::ranges::clamp(d, config.min, config.max);
  }

private:
  run_mode _mode;
  duty_val _duty{};
  duty_val _duty_next{};

  duty_val _dest{};  // destination duty when traveling
  float _velocity{}; // change per frame when fx is active
};

} // namespace desk
} // namespace pierre
