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
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/system.hpp>
#include <future>
#include <list>
#include <memory>
#include <mutex>

namespace pierre {

// forward decls to hide implementation details
class Anchor;

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using strand_ioc = asio::strand<asio::io_context::executor_type>;
using system_timer = asio::system_timer;
using work_guard_ioc = asio::executor_work_guard<asio::io_context::executor_type>;

class Racked {
private:
  using mem_order = std::memory_order;

public:
  Racked() noexcept;
  ~Racked() noexcept;

  void flush(FlushInfo &&request) noexcept;
  void flush_all() noexcept { flush(FlushInfo::make_flush_all()); }

  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  std::future<Reel> next_reel() noexcept;

  ssize_t reel_count() const noexcept { return std::ssize(reels); }

private:
  enum log_rc { NONE, RACKED };

private:
  void log_racked(log_rc rc = log_rc::NONE) const noexcept;
  void rack_wip() noexcept;

  Reel take_wip() noexcept;

private:
  // order dependent
  asio::io_context io_ctx;
  work_guard_ioc work_guard; // provides a service so requires a work_guard
  strand_ioc flush_strand;
  strand_ioc racked_strand;
  Reel wip;

  // order independent
  std::timed_mutex wip_mtx;
  FlushInfo flush_request;
  std::atomic_bool spool_frames{false};
  std::atomic<ssize_t> _reel_count{0};
  std::list<Reel> reels;

  ClockInfo clock_info;

public:
  static constexpr Nanos reel_max_wait{InputInfo::lead_time_min};
  static constexpr csv module_id{"desk.racked"};

}; // Racked

} // namespace pierre