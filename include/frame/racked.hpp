// Pierre - Custom Light Show for Wiss Landing
// Copyright (C) 2021  Tim Hughey
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
#include "frame/anchor_last.hpp"
#include "frame/clock_info.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"
#include "frame/reel.hpp"
#include "io/io.hpp"

#include <algorithm>
#include <atomic>
#include <future>
#include <latch>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>

namespace pierre {

// forward decls to hide implementation details
class Av;

using racked_reels = std::map<reel_serial_num_t, Reel>;

class Racked {

public:
  Racked(MasterClock *master_clock) noexcept;
  ~Racked() noexcept;

  void flush(FlushInfo &&request);
  void flush_all() noexcept { flush(FlushInfo::make_flush_all()); }

  // handeff() allows the packet to be moved however expects the key to be a reference
  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  /// @brief Get a shared_future to the next racked frame
  /// @return shared_future containing the next frame (could be silent)
  frame_future next_frame() noexcept;

  void spool(bool enable = true) noexcept {
    if (ready.load() == false) return;

    spool_frames.store(enable);
  }

private:
  enum log_racked_rc { NONE, RACKED, COLLISION, TIMEOUT };

private:
  void monitor_wip() noexcept;
  void rack_wip() noexcept;

  // misc logging, debug
  void log_racked(log_racked_rc rc = log_racked_rc::NONE) const noexcept;

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;
  strand handoff_strand;
  strand wip_strand;
  strand frame_strand;
  strand flush_strand;
  steady_timer wip_timer;
  MasterClock *master_clock;

  // order independent
  FlushInfo flush_request;
  std::atomic_bool ready{false};
  std::atomic_bool spool_frames{false};
  std::shared_timed_mutex rack_mtx;
  std::shared_timed_mutex wip_mtx;
  std::unique_ptr<Av> av;

  racked_reels racked;
  std::optional<Reel> wip;
  frame_t first_frame;

private:
  std::optional<std::latch> shutdown_latch;
  static int64_t REEL_SERIAL_NUM; // ever incrementing, no dups

public:
  static constexpr Nanos reel_max_wait{InputInfo::lead_time_min};
  static constexpr csv module_id{"desk.racked"};

}; // Racked

} // namespace pierre