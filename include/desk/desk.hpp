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
#include "frame/reel.hpp"

#include <atomic>
#include <boost/asio/error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system.hpp>
#include <boost/system/error_code.hpp>
#include <memory>

namespace pierre {

namespace asio = boost::asio;

using error_code = boost::system::error_code;
using strand_tp = asio::strand<asio::thread_pool::executor_type>;
using system_timer = asio::system_timer;

class Desk {

public:
  Desk(MasterClock *master_clock) noexcept; // must be defined in .cpp to hide FX includes

  ~Desk() noexcept; // must be in .cpp (incomplete types)

  void anchor_reset() noexcept;
  void anchor_save(AnchorData &&ad) noexcept;

  void flush(FlushInfo &&request) noexcept;

  void flush_all() noexcept;

  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  void set_render(bool enable) noexcept {
    auto was_active = std::atomic_exchange(&render_active, enable);

    if (was_active == false) {
      const auto now = system_clock::now();

      frame_timer.expires_at(now);
      frame_timer.async_wait([this](const error_code &ec) {
        if (!ec) asio::post([this]() { render(); });
      });
    }
  }

  void resume() noexcept;

private:
  bool fx_finished() const noexcept;
  void fx_select(Frame *frame) noexcept;

  void next_reel() noexcept;

  void render() noexcept;
  void render_start() noexcept;

  void set_active_reel(std::unique_ptr<Reel> &&reel) noexcept {
    std::exchange(active_reel, std::forward<std::unique_ptr<Reel>>(reel));
  }

  bool shutdown_if_all_stop() noexcept;

private:
  // order dependent
  const int thread_count;
  asio::thread_pool thread_pool;
  system_timer frame_timer;
  std::unique_ptr<Racked> racked;
  MasterClock *master_clock;

  // order independent
  std::atomic_bool render_active;
  std::unique_ptr<Anchor> anchor;
  std::unique_ptr<desk::DmxCtrl> dmx_ctrl{nullptr};
  std::unique_ptr<desk::FX> active_fx{nullptr};
  std::unique_ptr<Reel> active_reel;

  Elapsed render_elapsed;

public:
  static constexpr csv module_id{"desk"};
};

} // namespace pierre
