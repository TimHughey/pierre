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
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/fdecls.hpp"
#include "frame/fdecls.hpp"

#include <atomic>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <thread>
#include <vector>

namespace pierre {

namespace asio = boost::asio;

using error_code = boost::system::error_code;
using strand_ioc = asio::strand<asio::io_context::executor_type>;

class Desk {
public:
  using frame_timer = asio::steady_timer;
  using loop_clock = frame_timer::clock_type;

public:
  Desk(MasterClock *master_clock) noexcept; // see .cpp (incomplete types)
  ~Desk() noexcept;                         // see .cpp (incomplete types)

  void anchor_reset() noexcept;
  void anchor_save(AnchorData &&ad) noexcept;

  void flush(FlushInfo &&request) noexcept;

  void flush_all() noexcept;

  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  void peers(const Peers &&p) noexcept;

  void rendering(bool enable) noexcept;

  void resume() noexcept;

private:
  bool frame_render(desk::frame_rr &frr) noexcept;

  void loop() noexcept;

  bool shutdown_if_all_stop() noexcept;

  void threads_start() noexcept;
  void threads_stop() noexcept;

private:
  // order dependent
  conf::token tokc;
  asio::io_context io_ctx;
  strand_ioc render_strand;
  strand_ioc flush_strand;
  frame_timer loop_timer;
  MasterClock *master_clock;
  std::unique_ptr<Anchor> anchor;
  std::unique_ptr<Reel> reel;
  std::unique_ptr<Flusher> flusher;

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
