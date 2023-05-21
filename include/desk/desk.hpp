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

#include "base/conf/token.hpp"
// #include "base/elapsed.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/fdecls.hpp"
#include "frame/fdecls.hpp"
#include "frame/flusher.hpp"
#include "frame/frame.hpp"
#include "frame/master_clock.hpp"
#include "frame/reel.hpp"

#include <atomic>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system.hpp>
#include <boost/system/error_code.hpp>
#include <latch>
#include <memory>
#include <mutex>
#include <thread>

namespace pierre {

namespace asio = boost::asio;

using error_code = boost::system::error_code;
using strand_ioc = asio::strand<asio::io_context::executor_type>;

class Desk {
public:
  using frame_timer = asio::steady_timer;
  using loop_clock = frame_timer::clock_type;
  // using loop_expiry = frame_timer::time_point;

  struct frame_rr {
    frame_rr(Frame &&f) noexcept : f(std::move(f)) {}

    Frame f;
    bool stop{false};

    bool abort() const noexcept { return stop; }
    bool ok() const noexcept { return !abort(); }

    void finish() noexcept {
      f.record_state();
      f.record_sync_wait();

      if (f.ready()) {
        f.mark_rendered();
      }
    }

    void operator()(Frame &&f) noexcept { f = std::move(f); }
    Frame &operator()() noexcept { return f; }
  };

public:
  Desk(MasterClock *master_clock) noexcept; // see .cpp (incomplete types)
  ~Desk() noexcept;                         // see .cpp (incomplete types)

  void anchor_reset() noexcept;
  void anchor_save(AnchorData &&ad) noexcept;

  void flush(FlushInfo &&request) noexcept;

  void flush_all() noexcept { flush(FlushInfo(FlushInfo::All)); }

  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  void peers(const auto &&p) noexcept { master_clock->peers(std::forward<decltype(p)>(p)); }

  void rendering(bool enable) noexcept;

  void resume() noexcept;

private:
  bool frame_render(frame_rr &frr) noexcept;

  bool fx_finished() const noexcept;
  void fx_select(Frame &frame) noexcept;

  void loop() noexcept;

  bool shutdown_if_all_stop() noexcept;

  void threads_start() noexcept;
  void threads_stop() noexcept;

private:
  // order dependent
  conf::token tokc;
  asio::io_context io_ctx;
  MasterClock *master_clock;
  std::unique_ptr<Anchor> anchor;
  strand_ioc render_strand;
  strand_ioc flush_strand;
  frame_timer loop_timer;
  Reel reel;
  Flusher flusher;

  // order independent
  std::atomic_flag resume_flag;
  std::atomic_flag render_flag;
  std::vector<std::jthread> threads;
  std::unique_ptr<desk::FX> active_fx{nullptr};
  std::unique_ptr<desk::DmxCtrl> dmx_ctrl;

public:
  MOD_ID("desk");
};

} // namespace pierre
