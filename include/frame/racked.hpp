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
#include "frame/types.hpp"

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

class Racked;
namespace shared {
extern std::optional<Racked> racked;
}

using racked_reels = std::list<Reel>;

class Racked {
public:
  Racked() noexcept
      : guard(io_ctx.get_executor()), // ensure io_ctx has work
        handoff_strand(io_ctx),       // unprocessed frame 'queue'
        wip_strand(io_ctx),           // guard work in progress reeel
        wip_timer(io_ctx),            // used to racked incomplete wip reels
        rack_access(1)                // acquired, guards racked container
  {}

  static void adjust_render_mode(bool render) { shared::racked->impl_adjust_render_mode(render); }
  static void anchor_save(bool render, AnchorData &&ad) {
    shared::racked->impl_anchor_save(render, std::forward<AnchorData>(ad));
  }

  bool empty() const noexcept { return size() == 0; }

  static void flush(FlushInfo request) { shared::racked->impl_flush(std::move(request)); }
  static void handoff(uint8v &packet) { shared::racked->impl_handoff(packet); }

  static void init();
  const string inspect() const noexcept;

  static frame_future next_frame(const Nanos lead_time) noexcept {
    auto prom = frame_promise();
    auto fut = prom.get_future().share();

    shared::racked->impl_next_frame(lead_time, std::move(prom));

    return fut;
  }

  static auto racked_size() noexcept { return shared::racked->_racked_size.load(); }
  static bool rendering() noexcept { return shared::racked->_rendering.test(); }

  static bool rendering_wait() noexcept {
    auto &guard = shared::racked->_rendering;

    if (guard.test() == false) {
      guard.wait(false);
    }

    return guard.test();
  }

  static bool not_rendering() noexcept { return shared::racked->_rendering.test() == false; }
  int_fast64_t size() const noexcept { return _racked_size.load(); }

private:
  void accept_frame(frame_t frame) noexcept;
  void add_frame(frame_t frame) noexcept;
  void impl_adjust_render_mode(bool render) noexcept {
    auto prev = _rendering.test();

    if (render) {
      _rendering.test_and_set();
      _rendering.notify_all();
    } else {
      _rendering.clear();
      _rendering.notify_all();
    }

    INFO(module_id, "ADJUST_RENDER", "was={} is={}\n", prev, render);
  }

  void impl_anchor_save(bool render, AnchorData &&ad);
  void impl_flush(FlushInfo request);
  void impl_handoff(uint8v &packet) noexcept;
  void impl_next_frame(const Nanos lead_time, frame_promise prom) noexcept;

  void init_self();

  void monitor_wip() noexcept;

  void pop_front_reel_if_empty() noexcept;

  void rack_wip() noexcept;

  bool racked_acquire(const Nanos max_wait = reel_max_wait) noexcept {
    return rack_access.try_acquire_for(max_wait);
  }

  void racked_release() noexcept { rack_access.release(); }

  void reel_wait() noexcept {
    if (racked_size() == 0) {
      INFO(module_id, "REEL_WAIT", "waiting for a reel reels={}\n", racked_size());
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

  // misc logging, debug
  void log_racked() const noexcept { return log_racked(string()); }
  void log_racked(const string &wip_info) const noexcept;

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;
  strand handoff_strand;
  strand wip_strand;
  steady_timer wip_timer;
  std::binary_semaphore rack_access;
  std::atomic_flag _rendering;
  std::atomic_int_fast64_t _racked_size{0};

  // order independent
  FlushInfo flush_request;

  racked_reels racked;
  std::optional<Reel> reel_wip;
  std::optional<timestamp_t> first_timestamp;
  std::optional<seq_num_t> first_seqnum;

  // threads
  Threads _threads;
  stop_tokens _stop_tokens;

private:
  static constexpr int THREAD_COUNT{3}; // handoff, wip, other (flush)
  static uint64_t REEL_SERIAL_NUM;

public:
  static constexpr Nanos reel_max_wait = InputInfo::lead_time;
  static constexpr csv module_id{"RACKED"};

}; // Racked

} // namespace pierre