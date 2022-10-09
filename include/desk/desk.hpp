/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#include "base/flush_request.hpp"
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "desk/fx.hpp"
#include "desk/session/ctrl.hpp"
#include "desk/session/data.hpp"
#include "desk/stats.hpp"
#include "frame/frame.hpp"
#include "frame/racked.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <stop_token>
#include <vector>

namespace pierre {

class Desk;
namespace shared {
extern std::optional<Desk> desk;
}

class Desk {
public:
  using anchor_timer = std::unique_ptr<steady_timer>;

public:
  Desk() noexcept; // must be defined in .cpp to hide FX includes

  void halt() {}

  static void init();

  bool silence() const { return active_fx && active_fx->matchName(fx::SILENCE); }

private:
  void frame_loop(const Nanos wait = Nanos::zero());
  void frame_loop_delay(const Nanos wait);
  void frame_loop_safe();
  void frame_loop_unsafe();

  void frame_render(frame_t frame);
  void init_self();
  void streams_deinit();
  void streams_init();

  // misc debug

  // log_frome_timer_error: return true if ec == success
  bool log_frame_timer_error(const error_code &ec, csv fn_id) const;
  void sync_next_frame(const Nanos wait = InputInfo::lead_time()) noexcept;

private:
  // order dependent
  io_context io_ctx;
  strand streams_strand;
  strand handoff_strand;
  strand frame_strand;
  steady_timer frame_timer;
  steady_timer frame_late_timer;
  Nanos frame_last;
  strand render_strand;
  shFX active_fx;
  Nanos latency;
  work_guard guard;
  desk::Stats run_stats;

  // order independent
  Threads threads;
  std::stop_token stop_token;
  std::optional<desk::Control> control;

  // last error tracking
  error_code ec_last_ctrl_tx;

private:
  // static thread info
  static constexpr auto TASK_NAME = "Desk";
  static constexpr auto DESK_THREADS = 6;

public:
  static constexpr csv module_id = "DESK";
};

} // namespace pierre
