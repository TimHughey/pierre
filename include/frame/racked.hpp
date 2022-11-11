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

#include "base/anchor_last.hpp"
#include "base/input_info.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"
#include "frame/reel.hpp"

#include <algorithm>
#include <atomic>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <stop_token>

namespace pierre {

using racked_reels = std::list<Reel>;

class Racked {
public:
  Racked() noexcept
      : guard(io::make_work_guard(io_ctx)), // ensure io_ctx has work
        handoff_strand(io_ctx),             // unprocessed frame 'queue'
        wip_strand(io_ctx),                 // guard work in progress reeel
        wip_timer(io_ctx)                   // used to racked incomplete wip reels
  {}

  auto size() const noexcept { return _racked_size.load(); } // defined early for auto

  bool empty() const noexcept { return size() == 0; }

  static void flush(FlushInfo request);
  static void handoff(uint8v packet);

  static void init();
  const string inspect() const noexcept;

  static frame_future next_frame() noexcept;

private:
  void accept_frame(frame_t frame) noexcept;
  void add_frame(frame_t frame) noexcept;

  void flush_impl(FlushInfo request);
  void handoff_impl(uint8v packet) noexcept;
  void next_frame_impl(frame_promise prom) noexcept;

  void init_self();

  void monitor_wip() noexcept;

  void rack_wip() noexcept;

  void reel_wait() noexcept {
    if (size() == 0) {
      INFO(module_id, "REEL_WAIT", "waiting for a reel reels={}\n", size());
      std::atomic_wait(&_racked_size, 0);
    }
  }

  auto update_racked_size() noexcept {
    _racked_size.store(std::ssize(racked));
    return _racked_size.load();
  }

  void update_reel_ready() noexcept {
    if (size() > 0) {
      std::atomic_notify_all(&_racked_size);
    }
  }

  void update_wip_size() noexcept {
    if (reel_wip.has_value()) {
      _wip_size.store(std::ssize(*reel_wip));
    } else {
      _wip_size.store(0);
    }
  }

  bool wip_empty() const noexcept { return _wip_size.load() == 0; }
  bool wip_contains_one_frame() const noexcept { return _wip_size.load() == 1; }

  // misc logging, debug
  void log_racked() const noexcept { return log_racked(string()); }
  void log_racked(const string &wip_info) const noexcept;

private:
  // order dependent
  io_context io_ctx;
  work_guard_t guard;
  strand handoff_strand;
  strand wip_strand;
  steady_timer wip_timer;

  // order independent
  std::shared_timed_mutex rack_mtx;
  std::atomic_int_fast64_t _racked_size{0};
  std::atomic_int_fast64_t _wip_size{0};
  FlushInfo flush_request;

  racked_reels racked;
  std::optional<Reel> reel_wip;
  std::optional<Nanos> render_start_at;
  frame_t first_frame;
  bool synced = false;

  // threads
  Threads _threads;
  stop_tokens _stop_tokens;

private:
  static constexpr int THREAD_COUNT{3}; // handoff, wip, other (flush)
  static uint64_t REEL_SERIAL_NUM;

public:
  static constexpr Nanos reel_max_wait = InputInfo::lead_time_min;
  static constexpr csv module_id{"RACKED"};

}; // Racked

} // namespace pierre