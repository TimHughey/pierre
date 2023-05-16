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
#include "base/logger.hpp"
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
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

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
  bool flush_active() const noexcept { return flush_request.active(); }
  void flush_all() noexcept { flush(FlushInfo(FlushInfo::All)); }

  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  void next_reel(Reel &dr) noexcept {
    INFO_AUTO_CAT("next_reel");

    if (flush_active()) {
      INFO_AUTO("flush active, aborting");
      return;
    }

    dr.take(reel);
  }

private:
  // order dependent
  const int nthreads{2};
  std::vector<std::jthread> threads;
  asio::io_context io_ctx;
  work_guard_ioc work_guard; // prevent io_ctx from stopping when no work
  strand_ioc frames_strand;
  FlushInfo flush_request;
  std::atomic_flag next_reel_busy;
  std::atomic_flag reel_ready;
  Reel reel;

  // order independent
  std::set<timestamp_t> ts_oos;
  std::optional<std::reference_wrapper<Reel>> xfer_reel_ref;
  Reel xfer_pending_reel;
  std::mutex mtx;

public:
  static constexpr csv module_id{"frame.racked"};

}; // Racked

} // namespace pierre