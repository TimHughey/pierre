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

#include "base/elapsed.hpp"
#include "base/pe_time.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "desk/fx.hpp"
#include "desk/session/ctrl.hpp"
#include "desk/session/data.hpp"
#include "desk/stats.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"
#include "mdns/mdns.hpp"
#include "rtp_time/anchor/data.hpp"

#include <array>
#include <atomic>
#include <exception>
#include <memory>
#include <optional>
#include <ranges>
#include <stop_token>
#include <vector>

namespace pierre {

class Desk;
Desk *idesk();

class Desk {
public:
  static constexpr csv PLAYING{"playing"};
  static constexpr csv NOT_PLAYING{"not playing"};

private:
  Desk(const Nanos &lead_time); // must be defined in .cpp to hide FX includes

public: // instance API
  static void init(const Nanos &lead_time);

public: // general API
  void adjust_play_mode(csv next_mode);
  void halt() { adjust_play_mode(NOT_PLAYING); }
  bool playing() const { return play_mode.front() == PLAYING.front(); }
  void save_anchor_data(anchor::Data data); // see .cpp
  bool silence() const { return active_fx && active_fx->matchName(fx::SILENCE); }

  // misc debug
  static constexpr csv moduleID() { return module_id; }

private:
  void frame_next() { frame_next(Nanos(lead_time / 2), 1ms); } // only for first call
  void frame_next(Nanos sync_wait, Nanos lag);                 // frame next loop
  void frame_release(shFrame frame);
  void frame_render(shFrame frame);

  void streams_deinit();
  void streams_init();

  void next_frame(shFrame frame); // calc timing of next frame

  // misc debug
  void log_despooled(shFrame frame, Elapsed &elapsed);

private:
  // order dependent
  Nanos lead_time;
  io_context io_ctx;
  strand frame_strand;
  steady_timer frame_timer;
  strand streams_strand;
  steady_timer release_timer;
  shFX active_fx;
  Nanos latency;
  uint8v work_buff;
  work_guard guard;
  string_view play_mode;
  desk::stats stats;

  // order independent
  Threads threads;
  std::stop_token stop_token;

  std::optional<desk::Control> control;

  // last error tracking
  error_code ec_last_ctrl_tx;

  static constexpr csv module_id = "DESK";
  static constexpr auto TASK_NAME = "Desk";
  static constexpr auto DESK_THREADS = 4;
};

} // namespace pierre
