
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

#include "InfluxDBFactory.h"
#include "base/asio.hpp"
#include "base/conf/token.hpp"
#include "base/pet_types.hpp"
#include "base/stats/vals.hpp"
#include "base/types.hpp"

#include <InfluxDBFactory.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <chrono>
#include <cmath>
#include <concepts>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <type_traits>

namespace pierre {

class Stats;

extern std::unique_ptr<Stats> _stats;

/// @brief Write metrics to the timeseries database
///        serialized and thread safe. This class is
///        intended to be a singleton.
class Stats {

private:
  static constexpr auto def_db_uri{"http://localhost:8086?db=pierre"sv};

  static constexpr auto DOUBLE{"double"sv};
  static constexpr auto INTEGRAL{"integral"sv};
  static constexpr auto MEASURE{"STATS"sv};
  static constexpr auto METRIC{"metric"sv};
  static constexpr auto NANOS{"nanos"sv};

public:
  /// @brief Construct the Stats object.
  ///        Do not call this directly, use create
  /// @param app_io_ctx io_context for serialization
  Stats(asio::io_context &app_io_ctx) noexcept;
  ~Stats() = default;

  /// @brief Static member function to create the Stats singleton
  /// @param app_io_ctx io_context for serialization
  /// @return raw pointer to Stats singleton
  static auto create(asio::io_context &app_io_ctx) noexcept {
    _stats = std::make_unique<Stats>(app_io_ctx);

    return _stats.get();
  }

  /// @brief Static member function to initiate shutdown
  static void shutdown() noexcept {
    if ((bool)_stats && _stats->db) _stats->db->flushBatch();
  }

  /// @brief Write a metric to the timeseries db. All writes are serialized
  ///        and thread safe. The actual timeseries data point is created
  ///        prior to submission to the writer thread implying overhead
  ///        for the caller.
  /// @tparam T type of the value to write.
  ///         Supported types:
  ///         1) std::chrono::duration (converted to integral nanos),
  ///         2) bool (converted to integral 0 or 1),
  ///         3) signed and unsigned integrals (converted to signed)
  ///         4) floating point (converted to double)
  /// @param vt enumerated metric (see stats/vals.hpp)
  /// @param v metric val (from supported list of types)
  /// @param tag optional metric tag
  template <typename T>
  static void write(stats::stats_v vt, T v,
                    std::pair<const char *, const char *> tag = {nullptr, nullptr}) noexcept {

    if (auto s = _stats.get(); (s != nullptr) && s->db && s->enabled()) {

      // enabling stats incusr some overhead on the caller -- creation of the data point
      // and the call to asio::post
      auto pt = influxdb::Point(MEASURE.data()).addTag(METRIC.data(), s->val_txt[vt].data());

      // deliberately convert various types (e.g. chrono durations,
      // Frequency, Magnitude) to the correct type for influx and sets the field key to
      // an appropriate name to prevent different data types associated to the same key
      // (violates influx design)

      if constexpr (IsDuration<T>) {
        const auto d = std::chrono::duration_cast<Nanos>(v);
        pt.addField(NANOS, d.count());
      } else if constexpr (std::same_as<T, bool>) {
        pt.addField(INTEGRAL, v);
      } else if constexpr (std::unsigned_integral<T>) {
        using SignedT = std::make_signed<T>::type;
        pt.addField(INTEGRAL, static_cast<SignedT>(v));
      } else if constexpr (std::signed_integral<T>) {
        pt.addField(INTEGRAL, v);
      } else if constexpr (std::convertible_to<T, double>) {
        pt.addField(DOUBLE, v); // convert to double (e.g. Frequency, Magnitude)
      } else {
        static_assert(AlwaysFalse<T>, "unhandled type");
      }

      if (tag.first && tag.second) pt.addTag(tag.first, tag.second);

      // now post the pt and params for eventual write to influx
      asio::post(s->app_io_ctx, [s = s, pt = std::move(pt)]() mutable {
        s->async_write(std::move(pt)); // in .cpp due to call to Config
      });
    }
  }

private:
  void async_write(influxdb::Point &&pt) noexcept;
  bool enabled() noexcept;

private:
  // order dependent
  conf::token tokc;
  asio::io_context &app_io_ctx;
  std::map<stats::stats_v, string> val_txt;

  // order independent
  string db_uri;

  // order independent
  std::unique_ptr<influxdb::InfluxDB> db;

public:
  string init_msg;
  string err_msg;

public:
  static constexpr csv module_id{"stats"};
};

} // namespace pierre
