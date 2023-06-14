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

#include "base/asio.hpp"
#include "base/conf/token.hpp"
#include "base/dura_t.hpp"
#include "base/stats/vals.hpp"
#include "base/types.hpp"

#include <InfluxDBFactory.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <array>
#include <memory>
#include <tuple>

namespace pierre {

template <typename> constexpr bool StatsEnabled = true;

using stats_tag_t = std::array<const char *, 2>;

template <typename T>
concept IsStatsVal = IsDuration<T> || std::same_as<T, bool> || std::convertible_to<T, int64_t> ||
                     std::same_as<T, double> || std::convertible_to<T, double>;

template <typename T>
concept StatsCapable = requires(T v) {
  { v.tag() } noexcept -> std::same_as<stats_tag_t>;
  { v.stat() } noexcept -> IsStatsVal;
};

class Stats;

extern std::unique_ptr<Stats> _stats;

/// @brief Write metrics to the timeseries database
///        serialized and thread safe. This class is
///        intended to be a singleton.
class Stats {

private:
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
    if ((bool)_stats && (bool)_stats->db) _stats->db->flushBatch();
  }

  /// @brief Write a metric to the timeseries db. All writes are serialized
  ///        and thread safe. The actual timeseries data point is created
  ///        prior to submission to the writer thread implying overhead
  ///        for the caller.
  /// @tparam T type of the value to write.
  ///          Supported types:
  ///           1) std::chrono::duration (converted to integral nanos),
  ///           2) bool (converted to integral 0 or 1),
  ///           3) signed and unsigned integrals (converted to signed)
  ///           4) floating point (converted to double)
  /// @param vt enumerated metric (see stats/vals.hpp)
  /// @param v metric val (from supported list of types)
  /// @param tag optional metric tag
  template <typename T, typename V>
    requires IsStatsVal<V>
  inline static void write(stats::stats_v vt, V v, stats_tag_t tag) noexcept {

    if constexpr (StatsEnabled<T>) {
      if ((bool)_stats && (bool)_stats->db && _stats->enabled()) {
        // enabling stats incurs some overhead on the caller -- creation of the data point
        // and the call to asio::post
        auto pt = influxdb::Point(MEASURE.data()).addTag(METRIC.data(), val_txt[vt].data());

        // convert various types (e.g. chrono durations, Frequency, dB) to the
        // correct type for influx and sets the field key to ensure each metric key
        // is only associated to one type otherwise we violate influx data rules

        if constexpr (IsDuration<V>) {
          pt.addField(NANOS, std::chrono::duration_cast<Nanos>(v).count());
        } else if constexpr (std::same_as<V, bool>) {
          pt.addField(INTEGRAL, v);
        } else if constexpr (std::unsigned_integral<V>) {
          using Signed = std::make_signed<V>::type;
          pt.addField(INTEGRAL, static_cast<Signed>(v));
        } else if constexpr (std::signed_integral<V>) {
          pt.addField(INTEGRAL, v);
        } else if constexpr (std::convertible_to<V, double>) {
          pt.addField(DOUBLE, v); // convert to double (e.g. Frequency, dB)
        }

        if (tag[0] && tag[1]) pt.addTag(tag[0], tag[1]);

        // now post the pt and params for serialized write to influx
        auto &io_ctx = _stats->app_io_ctx;
        asio::post(io_ctx, [pt = std::move(pt)]() mutable { _stats->db->write(std::move(pt)); });
      }
    }
  }

  template <typename T, typename V>
    requires IsStatsVal<V> || StatsCapable<V>
  inline static void write(stats::stats_v vt, V v) noexcept {
    if constexpr (StatsCapable<V>) {
      write<T>(vt, v.stat(), v.tag());
    } else {
      write<T>(vt, v, stats_tag_t{});
    }
  }

private:
  bool enabled() noexcept;

private:
  // order dependent
  conf::token tokc;
  asio::io_context &app_io_ctx;

  // order independent
  string db_uri;
  std::unique_ptr<influxdb::InfluxDB> db;
  static std::map<stats::stats_v, string> val_txt;

public:
  string init_msg;
  string err_msg;

public:
  MOD_ID("stats");
};

} // namespace pierre
