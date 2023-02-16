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

#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "frame/clock_info.hpp"
#include "frame/racked.hpp"
#include "io/io.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <optional>

namespace pierre {

// forward decls to hide implementation details
class DmxCtrl;
class FX;
class Racked;

class Desk {

public:
  Desk(MasterClock *master_clock) noexcept; // must be defined in .cpp to hide FX includes
  ~Desk() noexcept;

  void flush(FlushInfo &&request) noexcept {
    if (racked.has_value()) {
      racked->flush(std::forward<FlushInfo>(request));
    }
  }

  void flush_all() noexcept {
    if (racked.has_value()) racked->flush_all();
  }

  void handoff(uint8v &&packet, const uint8v &key) noexcept {
    if (racked.has_value()) racked->handoff(std::forward<uint8v>(packet), key);
  }

  void resume() noexcept;

  void spool(bool enable = true) noexcept {
    if (racked.has_value()) racked->spool(enable);
  }

  void standby() noexcept;

private:
  void frame_loop() noexcept;

private:
  enum state_t : int { Running = 0, Stopped };

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;
  steady_timer frame_timer;
  MasterClock *master_clock;
  std::atomic_bool loop_active{false};
  std::atomic<state_t> state;
  const int thread_count;

  // order independent
  std::mutex run_state_mtx;
  std::optional<Racked> racked;
  std::unique_ptr<std::latch> shutdown_latch;

  std::unique_ptr<DmxCtrl> dmx_ctrl{nullptr};
  std::unique_ptr<FX> active_fx{nullptr};

public:
  static constexpr csv module_id{"desk"};
  static constexpr auto TASK_NAME{"desk"};
};

} // namespace pierre
