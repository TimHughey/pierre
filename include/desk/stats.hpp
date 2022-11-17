
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

#include "base/accum.hpp"
#include "base/elapsed.hpp"
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"

#include <concepts>
#include <map>
#include <memory>
#include <type_traits>
#include <variant>

namespace pierre {
namespace desk {

enum stats_v {
  CLOCKS_DIFF = 0,
  CTRL_CONNECT_ELAPSED,
  CTRL_CONNECT_TIMEOUT,
  CTRL_MSG_READ_ELAPSED,
  CTRL_MSG_READ_ERROR,
  CTRL_MSG_WRITE_ELAPSED,
  CTRL_MSG_WRITE_ERROR,
  DATA_MSG_WRITE_ERROR,
  DATA_MSG_WRITE_ELAPSED,
  FPS,
  FRAMES_RENDERED,
  FRAMES_SILENT,
  FRAMES,
  FREQUENCY,
  MAGNITUDE,
  NEXT_FRAME,
  NO_CONN,
  REELS_RACKED,
  REMOTE_DATA_WAIT,
  REMOTE_ELAPSED,
  REMOTE_ROUNDTRIP,
  RENDER_DELAY,
  RENDER_ELAPSED,
  RENDER,
  STREAMS_DEINIT,
  STREAMS_INIT,
  SYNC_WAIT,
};

class Stats : public std::enable_shared_from_this<Stats> {
private:
  Stats(io_context &io_ctx, csv measure, const string db_uri) noexcept;
  using stat_variant = std::variant<int32_t, int64_t, double>;

public:
  ~Stats() noexcept;
  static std::shared_ptr<Stats> create(io_context &io_ctx, csv measure,
                                       const string db_uri) noexcept;
  std::shared_ptr<Stats> init() noexcept;
  static void shutdown() noexcept;

  static void write(stats_v vt, auto v) noexcept {

    if constexpr (std::is_same_v<decltype(v), Elapsed>) v.freeze();

    if constexpr (std::is_same_v<decltype(v), Elapsed>) {
      write_stat(vt, stat_variant{v().count()}, "elapsed");

    } else if constexpr (std::is_same_v<decltype(v), Nanos> ||
                         std::is_same_v<decltype(v), Micros> ||
                         std::is_same_v<decltype(v), Millis>) {
      const auto d = pet::as<Nanos, decltype(v)>(v);
      write_stat(vt, stat_variant{d.count()}, "nanos");

    } else if constexpr (std::is_same_v<decltype(v), bool>) {
      write_stat(vt, stat_variant{v == true ? 1 : 0}, "boolean");

    } else if constexpr (std::is_same_v<decltype(v), int64_t> ||
                         std::is_same_v<decltype(v), int32_t>) {
      write_stat(vt, stat_variant{v}, "integer");

    } else if constexpr (std::is_same_v<decltype(v), float> ||
                         std::is_same_v<decltype(v), double> ||
                         std::is_convertible_v<decltype(v), double>) {
      write_stat(vt, stat_variant{v}, "floating_point");
    } else {
      write_stat(vt, stat_variant(v), "auto");
    }
  }

private:
  static void write_stat(stats_v vt, stat_variant sv, csv type) noexcept;

private:
  // order dependent
  const string db_uri;
  const string measurement;
  strand stats_strand;
  std::map<stats_v, string> val_txt;

public:
  static constexpr csv module_id{"DESK_STATS"};
};

} // namespace desk
} // namespace pierre