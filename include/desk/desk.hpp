/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "desk/fx.hpp"
#include "desk/session/ctrl.hpp"
#include "desk/session/data.hpp"
#include "desk/stats.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace pierre {

class Desk;
namespace shared {
extern std::optional<Desk> desk;
}

class Desk {
public:
  static constexpr csv RENDERING{"rendering"};
  static constexpr csv NOT_RENDERING{"not rendering"};

  using anchor_timer = std::unique_ptr<steady_timer>;

public:
  Desk(const Nanos &lead_time); // must be defined in .cpp to hide FX includes

public: // instance API
  static void init(const Nanos &lead_time);

public: // general API
  void adjust_mode(csv next_mode);

  static void anchor_save(AnchorData &&ad) {
    shared::desk->adjust_mode(Frame::anchor_save(std::forward<AnchorData>(ad)));
  }

  void halt() { adjust_mode(NOT_RENDERING); }
  bool rendering() const { return render_mode.front() == RENDERING.front(); }

  bool silence() const { return active_fx && active_fx->matchName(fx::SILENCE); }

  // misc debug
  static constexpr csv moduleID() { return module_id; }

private:
  // frame_first():
  //  1. called when rendering begins
  //  2. finds first frame
  //  3. calculates sync_timing
  //  4. invokes frame_next()
  void frame_first();
  void frame_next(); // frame next loop
  void frame_render(shFrame frame);

  void streams_deinit();
  void streams_init();

  // void wait_for_anchor(Nanos poll = 100ms, anchor_timer timer = nullptr);

  // misc debug

  // log_despooled(): returns true if frame is not null
  bool log_despooled(shFrame frame, const Nanos elapsed);

  // log_frome_timer_error: return true if ec == success
  bool log_frame_timer_error(const error_code &ec, csv fn_id) const;

private:
  // order dependent
  const Nanos lead_time;
  const Nanos lead_time_50;
  io_context io_ctx;
  steady_timer frame_timer;
  strand streams_strand;
  steady_timer release_timer;
  shFX active_fx;
  Nanos latency;
  work_guard guard;
  string_view render_mode;
  desk::stats stats;

  // order independent
  Threads threads;
  std::stop_token stop_token;
  std::optional<desk::Control> control;
  std::atomic_bool anchor_available = false;

  // last error tracking
  error_code ec_last_ctrl_tx;

  static constexpr csv module_id = "DESK";
  static constexpr auto TASK_NAME = "Desk";
  static constexpr auto DESK_THREADS = 4;
};

} // namespace pierre
