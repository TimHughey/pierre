
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
#include "stats/stats_v.hpp"

#include <InfluxDBFactory.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <tuple>

namespace pierre {

class Stats : public std::enable_shared_from_this<Stats> {
private:
  Stats(bool enabled = false) noexcept;
  using stat_variant = std::variant<int32_t, int64_t, double>;

public:
  ~Stats() noexcept {
    if (db) db.reset();
  }

  // can safely be called multiple times
  static void init() noexcept; // must be in .cpp to limit Config visibility

  static void shutdown(io_context &io_ctx) noexcept;

  template <typename T>
  static void write(stats::stats_v vt, T v,
                    std::pair<const char *, const char *> tag = {nullptr, nullptr}) noexcept {
    static constexpr csv DOUBLE{"double"};
    static constexpr csv INTEGRAL{"integral"};
    static constexpr csv MEASURE{"STATS"};
    static constexpr csv METRIC{"metric"};
    static constexpr csv NANOS{"nanos"};

    auto s = self()->shared_from_this(); // get our own reference counted shared_ptr

    if (s->enabled.load()) {

      // capture the time in the lambda so the point is most accurately recorded
      auto pt = influxdb::Point(MEASURE.data()).addTag(METRIC.data(), s->val_txt[vt].data());

      // now post the pt and params for eventual write to influx
      auto &io_ctx = s->io_ctx;
      asio::post(io_ctx, [s = std::move(s), v = std::move(v), pt = std::move(pt),
                          tag = std::move(tag)]() mutable {
        // this closure deliberately converts various types (e.g. chrono durations,
        // Frequency, Magnitude) to the correct type for influx and sets the field key to
        // an appropriate name to prevent different data types associated to the same key

        if constexpr (IsDuration<T>) {
          const auto d = std::chrono::duration_cast<Nanos>(v);
          pt.addField(NANOS, d.count());
        } else if constexpr (std::is_integral_v<T>) {
          pt.addField(INTEGRAL, v);
        } else if constexpr (std::is_convertible_v<T, double>) {
          pt.addField(DOUBLE, v); // convert to double (e.g. Frequency, Magnitude)
        } else {
          static_assert(always_false_v<T>, "unhandled type");
        }

        if (tag.first && tag.second) {
          pt.addTag(tag.first, tag.second);
        }

        s->db->write(std::move(pt));
      });
    }
  }

private:
  void init_self(const string &db_uri) noexcept;

  // DRAGONS BE HERE!!
  // returns reference to ACTUAL shared_ptr holding Stats
  static std::shared_ptr<Stats> &self() noexcept; // must be .cpp to access shared_ptr

private:
  // order dependent
  io_context io_ctx;
  work_guard_t guard;
  std::atomic_bool enabled{false};
  std::map<stats::stats_v, string> val_txt;

  // order independent
  std::unique_ptr<influxdb::InfluxDB> db;
  std::set<std::shared_ptr<influxdb::Point>> points;
  Threads threads;
  stop_tokens tokens;

public:
  static constexpr csv module_id{"PIERRE_STATS"};
};

} // namespace pierre
