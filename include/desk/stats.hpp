
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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#pragma once

#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "io/io.hpp"

#include <memory>

namespace pierre {
namespace desk {

class stats {
public:
  stats(io_context &io_ctx, Nanos interval) //
      : io_ctx(io_ctx), interval(interval), timer(io_ctx) {}

  void async_report(const Nanos report_interval) {
    timer.expires_after(report_interval);
    timer.async_wait([this](const error_code ec) {
      if (!ec) {
        __LOG0(LCOL01 " frames handled={:>6} none={:>6}\n", module_id, "REPORT", handled, none);

        async_report(interval);
      }
    });
  }

  void cancel() { timer.cancel(); }

  uint64_t handled = 0;
  uint64_t none = 0;

private:
  // order dependent
  io_context &io_ctx;
  Nanos interval;
  steady_timer timer;

  static constexpr csv module_id{"DESK STATS"};
};

} // namespace desk
} // namespace pierre