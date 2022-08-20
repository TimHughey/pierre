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
#include "base/input_info.hpp"
#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "io/io.hpp"
#include "player/spooler.hpp"

#include <memory>
#include <optional>
#include <set>

namespace pierre {
namespace player {

class Render {
public:
  Render(io_context &io_ctx, Spooler &spooler);

public: // public API
  void adjust_play_mode(csv mode);

private:
  void next_frame(Nanos sync_wait = 1ms, Nanos lag = 1ms);

  bool playing() const { return play_mode.front() == PLAYING.front(); }

  // misc debug, stats
  static constexpr csv moduleID() { return moduleId; }
  void report_stats(const Nanos interval = Nanos::zero());

private:
  // order dependent
  io_context &io_ctx;
  Spooler &spooler;
  strand &local_strand;
  steady_timer handle_timer;
  steady_timer release_timer;
  steady_timer stats_timer;

  Nanos lead_time;

  // order independent
  string_view play_mode = NOT_PLAYING;
  uint64_t frames_handled = 0;
  uint64_t frames_none = 0;
  Elapsed uptime;

  static constexpr auto moduleId = csv("RENDER");
  static constexpr Nanos STATS_INTERVAL = 10s;
};
} // namespace player
} // namespace pierre
