
//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#pragma once

#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"

#include <array>
#include <map>
#include <memory>

namespace pierre {
namespace desk {

enum stats_val {
  FRAMES = 0,
  FEEDBACKS,
  NO_CONN,
  STREAMS_DEINIT,
  STREAMS_INIT,
  RENDER,
  SYNC_WAIT_ZERO
};

class stats {
public:
  stats(io_context &io_ctx, Nanos interval) //
      : io_ctx(io_ctx),                     // save a ref to the callers io_ctx
        stats_strand(io_ctx),               // isolated strand for stats activities
        interval(interval),                 // frequenxy of stats reports
        timer(io_ctx),                      // timer for stats reporting
        val_txt({                           // create map of stats val to text
                 {FRAMES, "frames"},
                 {FEEDBACKS, "feedbacks"},
                 {NO_CONN, "no_conn"},
                 {STREAMS_DEINIT, "streams_deinit"},
                 {STREAMS_INIT, "streams_init"},
                 {RENDER, "render"},
                 {SYNC_WAIT_ZERO, "sync_wait_zero"}}) {}

  void cancel() noexcept {
    asio::post(stats_strand, [this]() { timer.cancel(); });
  }

  void operator()(stats_val val, uint32_t inc = 1) noexcept {
    asio::post(stats_strand, [=, this]() {
      _map[val] += inc; // adds val to map if it does not exist
    });
  }

  void start(const Nanos i = 10s) noexcept { async_report(i); }

private:
  void async_report(const Nanos i) noexcept {
    static const auto order = //
        std::array{FRAMES,  RENDER,       FEEDBACKS,     SYNC_WAIT_ZERO,
                   NO_CONN, STREAMS_INIT, STREAMS_DEINIT};

    interval = i; // cache the interval

    timer.expires_after(interval);
    timer.async_wait( //
        asio::bind_executor(stats_strand, [this](const error_code ec) {
          if (!ec) {
            string msg;
            auto w = std::back_inserter(msg);

            for (const auto &x : order) {
              const auto &val = _map[x];

              if (val > 0) fmt::format_to(w, "{}={:>7} ", val_txt[x], val);
            }

            if (!msg.empty()) __LOG0(LCOL01 " {}\n", module_id, "REPORT", msg);

            async_report(interval);
          }
        }));
  }

private:
  // order dependent
  io_context &io_ctx;
  strand stats_strand;
  Nanos interval;
  steady_timer timer;
  std::map<stats_val, string> val_txt;

  // order independent
  std::map<stats_val, uint64_t> _map;

public:
  static constexpr csv module_id{"DESK_STATS"};
};

} // namespace desk
} // namespace pierre