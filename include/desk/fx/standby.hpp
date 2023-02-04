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

#include "base/types.hpp"
#include "desk/color.hpp"
#include "desk/fx.hpp"
#include "io/io.hpp"
#include "lcs/config.hpp"

#include <memory>

namespace pierre {
namespace fx {

class Standby : public FX {

public:
  Standby(io_context &io_ctx)
      : io_ctx(io_ctx), silence_timer(io_ctx), next_color(Color()), silence_timeout{0} {}

  ~Standby() override final;

  void cancel() noexcept override {
    try {
      silence_timer.cancel();
    } catch (...) {
    }
  }

  void execute(Peaks &peaks) noexcept override;
  csv name() const override { return fx_name::STANDBY; };

  void once() override;

private:
  void load_config() noexcept;
  void silence_watch() noexcept;

private:
  // order dependent
  io_context &io_ctx;
  steady_timer silence_timer;
  Color next_color;
  Seconds silence_timeout;

  // order independent
  double hue_step{0.0};
  double max_brightness{0};
  double next_brightness{0};

  cfg_future cfg_change;

public:
  static constexpr csv module_id{"fx.standby"};
};

} // namespace fx
} // namespace pierre
