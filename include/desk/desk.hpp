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

#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/fdecls.hpp"
#include "frame/fdecls.hpp"
#include "io/context.hpp"
#include "io/timer.hpp"

#include <atomic>
#include <future>
#include <latch>
#include <memory>
#include <mutex>

namespace pierre {

class Desk {

public:
  Desk(MasterClock *master_clock) noexcept; // must be defined in .cpp to hide FX includes
  ~Desk() noexcept;

  void anchor_reset() noexcept;
  void anchor_save(AnchorData &&ad) noexcept;

  void flush(FlushInfo &&request) noexcept;

  void flush_all() noexcept;

  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  void resume() noexcept;

  void spool(bool enable = true) noexcept;

  void standby() noexcept;

private:
  void frame_loop(bool fx_finished = true) noexcept;
  void fx_select(const frame::state &frame_state, bool silent) noexcept;
  bool stopped() const noexcept { return state == Stopped; }
  bool running() const noexcept { return !io_ctx.stopped() && (state == Running); }

private:
  enum state_t : int { Running = 0, Stopped, Stopping };

private:
  // order dependent
  io_context io_ctx;
  steady_timer frame_timer;
  std::unique_ptr<Racked> racked;
  MasterClock *master_clock;
  std::unique_ptr<Anchor> anchor;
  std::atomic<state_t> state;
  const int thread_count;

  // order independent
  std::mutex run_state_mtx;
  std::unique_ptr<std::latch> shutdown_latch;

  std::unique_ptr<desk::DmxCtrl> dmx_ctrl{nullptr};
  std::unique_ptr<desk::FX> active_fx{nullptr};

public:
  static constexpr csv module_id{"desk"};
  static constexpr auto TASK_NAME{"desk"};
};

} // namespace pierre
