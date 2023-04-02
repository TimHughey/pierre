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

#include "base/elapsed.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/fdecls.hpp"
#include "frame/fdecls.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <memory>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using strand_tp = asio::strand<asio::thread_pool::executor_type>;
using steady_timer = asio::steady_timer;
using work_guard_tp = asio::executor_work_guard<asio::thread_pool::executor_type>;

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

  void spool(bool enable = true) noexcept; // must be in .cpp for Racked

private:
  void frame_loop(bool fx_finished = true) noexcept;
  void handle_frame(frame_t frame, bool fx_finished) noexcept;
  void fx_select(const frame::state &frame_state, bool silent) noexcept;

private:
  // order dependent
  const int thread_count;
  asio::thread_pool thread_pool;
  work_guard_tp guard;
  strand_tp frame_strand;
  strand_tp sync_strand;
  steady_timer frame_timer;
  std::unique_ptr<Racked> racked;
  MasterClock *master_clock;
  std::unique_ptr<Anchor> anchor;

  // order independent
  std::unique_ptr<desk::DmxCtrl> dmx_ctrl{nullptr};
  std::unique_ptr<desk::FX> active_fx{nullptr};

public:
  static constexpr csv module_id{"desk"};
  static constexpr auto TASK_NAME{"desk"};
};

} // namespace pierre
