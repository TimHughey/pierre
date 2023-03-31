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
#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "frame/anchor_last.hpp"
#include "frame/clock_info.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"
#include "frame/reel.hpp"
#include "io/strand.hpp"
#include "io/timer.hpp"
#include "io/work_guard.hpp"

#include <algorithm>
#include <atomic>
#include <future>
#include <latch>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>

namespace pierre {

// forward decls to hide implementation details
class Anchor;
class Av;

using racked_reels = std::list<Reel>;

class Racked {

public:
  Racked(MasterClock *master_clock, Anchor *anchor) noexcept;
  ~Racked() noexcept;

  void flush(FlushInfo &&request) noexcept;
  void flush_all() noexcept { flush(FlushInfo::make_flush_all()); }

  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  template <typename Executor, typename CompletionToken>
  void next_frame(Executor &handler_executor, CompletionToken &&token) noexcept {
    constexpr csv fn_id{"next_frame"};

    auto initiation = [](auto &&completion_handler, Executor &handler_executor, Racked &self) {
      struct ic_handler {
        Executor &handler_executor;
        Racked &self;
        typename std::decay<decltype(completion_handler)>::type handler;

        void operator()() noexcept { handler(self.next_frame_no_wait()); }

        using executor_type =
            asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                        typename Executor::executor_type>;

        executor_type get_executor() const noexcept {
          return asio::get_associated_executor(handler, handler_executor.get_executor());
        }

        using allocator_type =
            asio::associated_allocator_t<typename std::decay<decltype(completion_handler)>::type,
                                         std::allocator<void>>;

        allocator_type get_allocator() const noexcept {
          return asio::get_associated_allocator(handler, std::allocator<void>{});
        }

      }; // end struct ic_handler

      asio::post(self.racked_strand,
                 ic_handler{handler_executor, self,
                            std::forward<decltype(completion_handler)>(completion_handler)});
    }; // end initiation

    return asio::async_initiate<CompletionToken, void(frame_t frame)>(
        initiation, token, std::ref(handler_executor), std::ref(*this));
  }

  void spool(bool enable = true) noexcept { std::atomic_exchange(&spool_frames, enable); }

private:
  enum log_rc { NONE, RACKED };

private:
  void log_racked(log_rc rc = log_rc::NONE) const noexcept;

  void monitor_wip() noexcept;
  frame_t next_frame_no_wait() noexcept;

  void rack_wip() noexcept;

private:
  // order dependent
  const int thread_count;
  io_context io_ctx;
  work_guard guard;
  strand flush_strand;
  strand handoff_strand;
  strand wip_strand;
  strand racked_strand;
  system_timer wip_timer;
  MasterClock *master_clock;
  Anchor *anchor;

  // order independent
  FlushInfo flush_request;
  std::atomic_bool ready{false};
  std::atomic_bool spool_frames{false};
  std::unique_ptr<Av> av;

  ClockInfo clock_info;

  racked_reels racked;
  Reel wip;

private:
  std::shared_ptr<std::latch> shutdown_latch;

public:
  static constexpr Nanos reel_max_wait{InputInfo::lead_time_min};
  static constexpr csv module_id{"desk.racked"};

}; // Racked

} // namespace pierre