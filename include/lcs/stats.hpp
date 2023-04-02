
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

#include "base/pet_types.hpp"
#include "base/types.hpp"
#include "io/post.hpp"
#include "io/strand.hpp"
#include "lcs/stats_v.hpp"

#include <InfluxDBFactory.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <tuple>

namespace pierre {

class Stats;

namespace shared {
extern std::unique_ptr<Stats> stats;
}

class Stats {

public:
  Stats(io_context &io_ctx) noexcept;
  ~Stats() = default;

  template <typename T>
  static void write(stats::stats_v vt, T v,
                    std::pair<const char *, const char *> tag = {nullptr, nullptr}) noexcept {
    static constexpr csv DOUBLE{"double"};
    static constexpr csv INTEGRAL{"integral"};
    static constexpr csv MEASURE{"STATS"};
    static constexpr csv METRIC{"metric"};
    static constexpr csv NANOS{"nanos"};

    if (!shared::stats) return;

    auto s = shared::stats.get();
    auto &strand = s->stats_strand;

    if (s->enabled) {

      // enabling stats does incur some overhead, primarily the creation of the data point
      // and the call to asio::post
      auto pt = influxdb::Point(MEASURE.data()).addTag(METRIC.data(), s->val_txt[vt].data());

      // deliberately convert various types (e.g. chrono durations,
      // Frequency, Magnitude) to the correct type for influx and sets the field key to
      // an appropriate name to prevent different data types associated to the same key
      // (violates influx design)

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

      // now post the pt and params for eventual write to influx
      asio::post(strand, [s = s, pt = std::move(pt)]() mutable {
        s->async_write(std::move(pt)); // in .cpp due to call to Config
      });
    }
  }

private:
  void async_write(influxdb::Point &&pt) noexcept;

private:
  // order dependent
  strand stats_strand;
  std::atomic_bool enabled{false};
  const string db_uri;
  int batch_of;
  std::map<stats::stats_v, string> val_txt;

  // order independent
  std::unique_ptr<influxdb::InfluxDB> db;

public:
  string init_msg;
  string err_msg;

public:
  static constexpr csv module_id{"lcs.stats"};
};

} // namespace pierre
