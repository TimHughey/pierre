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

#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "frame/anchor_last.hpp"
#include "frame/clock_info.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"
#include "frame/reel.hpp"

#include <algorithm>
#include <atomic>
#include <boost/asio/error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <future>
#include <list>
#include <memory>

namespace pierre {

// forward decls to hide implementation details
class Anchor;
class Av;

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using strand_tp = asio::strand<asio::thread_pool::executor_type>;
using system_timer = asio::system_timer;

class Racked {
private:
  using mem_order = std::memory_order;

public:
  Racked() noexcept;
  ~Racked() noexcept;

  void flush(FlushInfo &&request) noexcept;
  void flush_all() noexcept { flush(FlushInfo::make_flush_all()); }

  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  std::future<std::unique_ptr<Reel>> next_reel() noexcept;

  ssize_t reel_count() const noexcept { return std::ssize(reels); }
  ssize_t wip_reels() const noexcept { return wip->size(); }

private:
  enum log_rc { NONE, RACKED };

private:
  void log_racked(log_rc rc = log_rc::NONE) const noexcept;
  void rack_wip(frame_t frame = frame_t()) noexcept;

  std::unique_ptr<Reel> take_wip() noexcept {
    return std::exchange(wip, std::make_unique<Reel>(Reel::DEFAULT_MAX_FRAMES));
  }

private:
  // order dependent
  const int thread_count;
  asio::thread_pool thread_pool;
  strand_tp flush_strand;
  strand_tp handoff_strand;
  strand_tp racked_strand;
  std::unique_ptr<Reel> wip;
  std::unique_ptr<Av> av;

  // order independent
  Elapsed recent_handoff;
  FlushInfo flush_request;
  std::atomic_bool spool_frames{false};
  std::atomic<ssize_t> _reel_count{0};
  std::list<std::unique_ptr<Reel>> reels;

  ClockInfo clock_info;

public:
  static constexpr Nanos reel_max_wait{InputInfo::lead_time_min};
  static constexpr csv module_id{"desk.racked"};

}; // Racked

} // namespace pierre