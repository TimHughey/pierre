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
#include "base/conf/toml.hpp"
#include "base/input_info.hpp"
#include "base/types.hpp"
#include "desk/unit.hpp"
#include <base/bound.hpp>

#include <array>
#include <cstdint>
#include <ranges>

namespace pierre {

namespace desk {

using duty_val = uint32_t;
using duty_percent = double;
using bound_duty = bound<double>;

class Dimmable : public Unit {

private:
  enum run_mode : uint8_t { Fixed = 0, PulseInit, PulseRun };

public:
  enum duty_vals : uint8_t { Bright = 0, Dest, Dim, Now, Next, EndOfDutyVals };

public:
  Dimmable(const toml::table &t) noexcept : Unit(t), dev_range({0, 8190}), _mode{Fixed} {

    // intialize default duty vals
    max_percent(Dim, 0.005);
    duties[Bright] = dev_range.second();

    t.for_each([this](const toml::key &key, auto &&elem) {
      if (key == "dim") {
        elem.visit([this](const toml::value<double> &v) { max_percent(v, duties[Dim]); });
      }

      elem.visit([&, this](const toml::array &arr) {
        if (key == "range") sub_range.assign(arr);
        if (key == "pulse") pulse_range.assign(arr);
      });
    });
  }

  duty_val duty() const { return _duty; }
  duty_val duty(duty_vals dv) const noexcept { return duties[dv]; }

  void activate() noexcept override final { fixed(duty(Bright)); }

  void stop() { fixed(dev_range.min()); }

public:
  void bright() { fixed(dev_range.get().max); }
  void dark() noexcept override final { fixed(dev_range.first()); }
  void dim() noexcept { fixed(duty(Dim)); }

  void fixed(const duty_val d) {
    duty_next(d);
    _mode = Fixed;
  }

  void percent(const duty_percent x) { fixed(x * dev_range.second()); }

  // void prepare() noexcept override final {}

  constexpr void max_percent(const toml::value<double> &vt, auto &v) noexcept {
    v = (duty_val)(dev_range.max() * vt.get());
  }

  constexpr void max_percent(duty_vals dv, double v) noexcept {
    duties[dv] = (duty_val)(duties[dv] * v);
  }

  static constexpr duty_val make_percent(bound<double> &bounds, double v) noexcept {
    return (duty_val)(bounds.second() * v);
  }

  /*void pulse(double intensity = 1.0, double secs = 0.2) {
    // intensity is the percentage of max brightness for the pulse
    auto start = pulse_range.first() * intensity;

    duty_next(start);
    _dest = (duty_val)pulse_range.max();

    // compute change per frame required to reach dark within requested secs
    _velocity = (start - _dest) / (InputInfo::fps * secs);

    _mode = PulseInit;
  }*/

  void update_msg([[maybe_unused]] DataMsg &msg) noexcept override final {
    // auto sub_max = dev_range.max() * sub_range.max();
    // auto level = duties[Next] *

    //              msg.add_kv(name, std::exchange(_duty, _duty_next));
  }

protected:
  void duty_next(duty_val d) noexcept {
    _duty_next = std::ranges::clamp(d, dev_range.min(), dev_range.max());
  }

public:
  bound<duty_val> dev_range;
  bound<double> sub_range;   // subclass specific min and max
  bound<double> pulse_range; // pulse min max

  std::array<duty_val, EndOfDutyVals> duties{};

private:
  run_mode _mode;
  duty_val _duty{};
  duty_val _duty_next{};

  duty_val _dest{};   // destination duty when traveling
  double _velocity{}; // change per frame when fx is active
};

} // namespace desk
} // namespace pierre
