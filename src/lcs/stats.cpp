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

#include "lcs/stats.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats_map.hpp"

#include <fmt/format.h>
#include <memory>

namespace pierre {

namespace shared {
std::unique_ptr<Stats> stats;
}

Stats::Stats(asio::thread_pool &thread_pool) noexcept
    : stats_strand(asio::make_strand(thread_pool)),       //
      enabled(config_val<Stats, bool>("enabled", false)), //
      db_uri(config_val<Stats, string>("db_uri", "http://localhost:8086?db=pierre")),
      batch_of(config_val<Stats, int>("batch_of", 150)), //
      val_txt{stats::make_map()}                         //
{
  auto w = std::back_inserter(init_msg);

  fmt::format_to(w, "sizeof={:>5} {} db_uri={} val_map={}",
                 sizeof(Stats),                    //
                 enabled ? "enabled" : "disabled", //
                 db_uri.empty() ? "<unset" : "<set>", val_txt.size());

  if (enabled && !db_uri.empty()) {

    fmt::format_to(w, " batch_of={}", batch_of);

    try {
      db = influxdb::InfluxDBFactory::Get(db_uri);

      if (db) db->batchOf(batch_of);
    } catch (const std::exception &err) {
      enabled = false;
      err_msg.assign(err.what());
    }

    if (!err_msg.empty()) fmt::format_to(w, " err={}", !err_msg.empty());
  }

  INFO_INIT("{}\n", init_msg);
}

void Stats::async_write(influxdb::Point &&pt) noexcept {

  if (db) db->write(std::forward<influxdb::Point>(pt));

  // updated enabled config
  // enabled = config_val<Stats, bool>("enabled", false);
}

} // namespace pierre
