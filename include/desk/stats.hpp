
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

#include "base/accum.hpp"
#include "base/elapsed.hpp"
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"

// #include <array>
// #include <boost/accumulators/statistics/max.hpp>
// #include <boost/accumulators/statistics/min.hpp>
// #include <boost/accumulators/statistics/rolling_count.hpp>
// #include <boost/accumulators/statistics/rolling_mean.hpp>
// #include <boost/accumulators/statistics/rolling_sum.hpp>
// #include <boost/accumulators/statistics/rolling_window.hpp>
#include <map>
// #include <memory>

namespace pierre {
namespace desk {

using namespace boost::accumulators;

enum stats_val {
  FEEDBACKS = 0,
  FRAMES,
  NEXT_FRAME,
  NO_CONN,
  REMOTE_ASYNC,
  REMOTE_ELAPSED,
  REMOTE_JITTER,
  REMOTE_LONG_ROUNDTRIP,
  RENDER,
  RENDER_DELAY,
  STREAMS_DEINIT,
  STREAMS_INIT,
  SYNC_WAIT,
  SYNC_WAIT_NEG,
  SYNC_WAIT_ZERO
};

class Stats {
public:
  Stats(io_context &io_ctx)
      : db_uri("http://localhost:8086?db=pierre"), // where we save stats
        stats_strand(io_ctx),                      // isolated strand for stats activities
        val_txt({                                  // create map of stats val to text
                 {FEEDBACKS, "feedbacks"},
                 {FRAMES, "frames"},
                 {NEXT_FRAME, "next_frame"},
                 {NO_CONN, "no_conn"},
                 {REMOTE_ASYNC, "remote_async"},
                 {REMOTE_ELAPSED, "remote_elapsed"},
                 {REMOTE_JITTER, "remote_jitter"},
                 {REMOTE_LONG_ROUNDTRIP, "remote_log_roundtrip"},
                 {RENDER, "render"},
                 {RENDER_DELAY, "render_delay"},
                 {STREAMS_DEINIT, "streams_deinit"},
                 {STREAMS_INIT, "streams_init"},
                 {SYNC_WAIT, "sync_wait"},
                 {SYNC_WAIT_NEG, "sync_wait_neg"},
                 {SYNC_WAIT_ZERO, "sync_wait_zero"}}) {}

  void operator()(stats_val v, int32_t x = 1) noexcept;
  void operator()(stats_val v, Elapsed &e) noexcept;
  void operator()(stats_val v, const Nanos d) noexcept;
  void operator()(stats_val v, const Micros d) noexcept;

  // void remote_async(uint32_t async_us) {

  // }

private:
  void flush_if_needed();
  void init_db_if_needed();
  // void raw_write_int64(stats_val v, int64_t x);

private:
  // order dependent
  const string db_uri;
  strand stats_strand;
  std::map<stats_val, string> val_txt;

public:
  static constexpr csv module_id{"DESK_STATS"};
};

} // namespace desk
} // namespace pierre