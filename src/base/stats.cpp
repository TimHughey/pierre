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

#include "base/stats.hpp"
#include "base/conf/toml.hpp"
#include "base/logger.hpp"
#include "base/stats/map.hpp"

#include <fmt/format.h>
#include <memory>

namespace pierre {

std::unique_ptr<Stats> _stats;

// class members
std::map<stats::stats_v, string> Stats::val_txt;

Stats::Stats(asio::io_context &app_io_ctx) noexcept
    : // initialize our conf token
      tokc(module_id),
      // create our config token
      app_io_ctx(app_io_ctx) //
{
  if (val_txt.empty()) val_txt = std::move(stats::make_map());

  db_uri = tokc.val<string>("db_uri", "http://influx:influx1234@localhost:8086?db=bucket0");

  auto w = std::back_inserter(init_msg);

  fmt::format_to(w, "sizeof={:>5} {} db_uri={} val_map={}", sizeof(Stats),
                 enabled() ? "enabled" : "disabled", db_uri.empty() ? "<unset" : "<set>",
                 val_txt.size());

  if (enabled() && !db_uri.empty()) {
    const auto batch_of = tokc.val<int>("batch_of", 150);

    fmt::format_to(w, " batch_of={}", batch_of);

    try {
      db = influxdb::InfluxDBFactory::Get(db_uri);
      db->batchOf(batch_of);

    } catch (const std::exception &e) {
      db.reset();
      err_msg.assign(e.what());
    }

    if (!err_msg.empty()) fmt::format_to(w, " err={}", !err_msg.empty());
  }

  INFO_INIT("{}\n", init_msg);
}

bool Stats::enabled() noexcept { return tokc.val<bool>("enabled", false); }

} // namespace pierre