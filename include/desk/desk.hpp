// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"
#include "desk/fx.hpp"
#include "desk/session/ctrl.hpp"
#include "desk/stats.hpp"
#include "frame/frame.hpp"
#include "frame/racked.hpp"

#include <atomic>
#include <memory>
#include <optional>

namespace pierre {

class Desk;
namespace shared {
extern std::optional<Desk> desk;
}

class Desk {
public:
  Desk() noexcept; // must be defined in .cpp to hide FX includes

  void halt() {}

  static void init() noexcept;

  bool silence() const noexcept { return active_fx && active_fx->matchName(fx::SILENCE); }

private:
  void frame_loop(Nanos wait = InputInfo::lead_time_min) noexcept;

  void frame_render(frame_t frame);
  void init_self();
  void streams_deinit();
  void streams_init();

  // misc debug

  // log_frome_timer_error: return true if ec == success
  bool log_frame_timer(const error_code &ec, csv fn_id) const;
  void log_init(int num_threads) const noexcept;

private:
  // order dependent
  io_context io_ctx;
  strand streams_strand;
  strand handoff_strand;
  strand frame_strand;
  strand render_strand;
  Nanos frame_last;
  shFX active_fx;
  work_guard_t guard;
  desk::Stats run_stats;
  std::atomic_bool loop_active;

  // order independent
  Threads threads;
  stop_tokens tokens;
  std::optional<desk::Control> control;

  // last error tracking
  error_code ec_last_ctrl_tx;

private:
  // static thread info
  static constexpr auto TASK_NAME{"Desk"};

public:
  static constexpr csv module_id{"DESK"};
};

} // namespace pierre
