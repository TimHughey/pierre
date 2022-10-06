
/*
    Possible
    Copyright (C) 2022  Tim Hughey

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

#include "base/anchor_last.hpp"
#include "base/flush_request.hpp"
#include "base/input_info.hpp"
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
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
        wip_strand(io_ctx),           // guard work in progress reeel
        wip_timer(io_ctx),            // used to racked incomplete wip reels
        future_timer(io_ctx),         // used to wait for future frame
        rack_access(1),               // held, guards racked container
        reel_ready(1)                 // held, denotes at least one frame available
  {}

  static void adjust_render_mode(bool render) { shared::racked->impl_adjust_render_mode(render); }
  static void anchor_save(bool render, AnchorData &&ad) {
    shared::racked->impl_anchor_save(render, std::forward<AnchorData>(ad));
  }

  bool empty() const noexcept { return size() == 0; }

  static void flush(FlushRequest request) { shared::racked->impl_flush(std::move(request)); }
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

    if (shared::racked->_rendering.test() == false) {
      shared::racked->_rendering.wait(false);
    }

    return shared::racked->_rendering.test();
  }

  static bool not_rendering() noexcept { return shared::racked->_rendering.test() == false; }
  int_fast64_t size() const noexcept { return _racked_size.load(); }

private:
  void impl_adjust_render_mode(bool render) noexcept {
    auto prev = _rendering.test();

    if (render) {
      _rendering.test_and_set();
    } else {
      _rendering.clear();
    }

    if (prev != _rendering.test()) {
      __LOG0(LCOL01 " mode={} rendering={} previous={}\n", //
             module_id, "ADJUST_RENDER", render, _rendering.test(), prev);
    }
  }

  void impl_anchor_save(bool render, AnchorData &&ad);
  void impl_flush(FlushRequest request);
  void impl_handoff(uint8v &packet);
  void impl_next_frame(const Nanos lead_time, frame_promise prom) noexcept;

  void init_self();

  void monitor_wip() noexcept;

  void pop_front_reel_if_empty() noexcept;

  bool racked_acquire(const Nanos max_wait = reel_max_wait) noexcept {
    return rack_access.try_acquire_for(max_wait);
  }
  void racked_release() noexcept { rack_access.release(); }

  void rack_wip() noexcept;

  auto update_racked_size() noexcept {
    _racked_size.store(std::ssize(racked));
    return _racked_size.load();
  }

  void update_reel_ready() noexcept;

  void wait_for_reel() noexcept {
    reel_ready.acquire();
    reel_ready.release();
  }

  // misc logging, debug
  void log_racked() const noexcept { return log_racked(string()); }
  void log_racked(const string &wip_info) const noexcept;

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;
  strand wip_strand;
  steady_timer wip_timer;
  steady_timer future_timer;
  std::binary_semaphore rack_access;
  std::binary_semaphore reel_ready;
  std::atomic_flag _rendering;
  std::atomic_int_fast64_t _racked_size{0};
  // order independent

  FlushRequest flush_request;

  racked_reels racked;
  std::optional<Reel> reel_wip;

  // threads
  Threads _threads;
  stop_tokens _stop_tokens;

private:
  static constexpr int THREAD_COUNT{3};
  static uint64_t REEL_SERIAL_NUM;

public:
  static constexpr Nanos reel_max_wait = InputInfo::lead_time();
  static constexpr csv module_id{"RACKED"};

}; // Racked

} // namespace pierre