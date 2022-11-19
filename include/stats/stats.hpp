
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

#pragma once

#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"

#include <InfluxDBFactory.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <memory>

namespace pierre {
namespace stats {

enum stats_v {
  CLOCK_DIFF,
  CTRL_CONNECT_ELAPSED,
  CTRL_CONNECT_TIMEOUT,
  CTRL_MSG_READ_ELAPSED,
  CTRL_MSG_READ_ERROR,
  CTRL_MSG_WRITE_ELAPSED,
  CTRL_MSG_WRITE_ERROR,
  DATA_MSG_WRITE_ELAPSED,
  DATA_MSG_WRITE_ERROR,
  FLUSH_ELAPSED,
  FLUSHED_REELS,
  FPS,
  FRAMES_RENDERED,
  FRAMES_SILENT,
  FRAMES,
  MAX_PEAK_FREQUENCY,
  MAX_PEAK_MAGNITUDE,
  NEXT_FRAME_WAIT,
  NO_CONN,
  RACK_COLLISION,
  RACK_WIP_TIMEOUT,
  RACKED_REELS,
  REMOTE_DATA_WAIT,
  REMOTE_ELAPSED,
  REMOTE_ROUNDTRIP,
  RENDER_DELAY,
  RENDER_ELAPSED,
  SYNC_WAIT,
  // extra comma allows for easy IDE sorting
};
}

class Stats : public std::enable_shared_from_this<Stats> {
private:
  Stats() noexcept;
  using stat_variant = std::variant<int32_t, int64_t, double>;

public:
  ~Stats() noexcept {
    if (db) db.reset();
  }

  // can safely be called multiple times
  static void init() noexcept;

  static void shutdown() noexcept;

  template <typename T> static auto write(stats::stats_v vt, T v) noexcept {
    auto pt = make_pt();

    // if constexpr (std::is_same_v<T, Elapsed>) {
    //   v.freeze();
    //   pt.addField(NANOS, v().count());
    // } else

    if constexpr (IsDuration<T>) {
      pt.addField(NANOS, std::chrono::duration_cast<Nanos>(v).count());
    } else if constexpr (std::is_convertible_v<T, Nanos>) {
      pt.addField(NANOS, Nanos(v).count());
    } else if constexpr (std::is_integral_v<T>) {
      pt.addField(INTEGRAL, v);
    } else if constexpr (std::is_convertible_v<T, double>) {
      pt.addField(DOUBLE, v);
    } else {
      static_assert("bad type");
    }

    ptr()->write_pt(vt, std::move(pt));
  }

private:
  void init_self(const string &db_uri) noexcept;

  static auto make_pt() noexcept -> influxdb::Point { return influxdb::Point(MEASURE); }

  static std::shared_ptr<Stats> ptr() noexcept;

  void write_pt(stats::stats_v vt, influxdb::Point pt) noexcept {
    pt.addTag(METRIC, val_txt[vt]);

    asio::post(io_ctx, [s = shared_from_this(), pt = std::move(pt)]() mutable {
      s->db->write(std::move(pt));
    });
  }

private:
  // order dependent
  io_context io_ctx;
  work_guard_t guard;
  std::map<stats::stats_v, string> val_txt;

  // order independent
  std::atomic_bool enabled{false};
  std::unique_ptr<influxdb::InfluxDB> db;
  Threads threads;
  stop_tokens tokens;

public:
  static constexpr csv module_id{"PIERRE_STATS"};

  static constexpr auto MEASURE{"STATS"};

  static constexpr csv DOUBLE{"double"};
  static constexpr csv INTEGRAL{"integral"};
  static constexpr csv METRIC{"metric"};
  static constexpr csv NANOS{"nanos"};
};

} // namespace pierre
