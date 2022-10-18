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

#include "desk/stats.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"

#include <InfluxDBFactory.h>
#include <memory>

namespace pierre {
namespace desk {

static std::unique_ptr<influxdb::InfluxDB> db;

void Stats::init_db_if_needed() {
  if (!db) {
    db = influxdb::InfluxDBFactory::Get(db_uri);

    INFO(module_id, "INIT_DB", "success db={}\n", fmt::ptr(db.get()));
  }
}

void Stats::operator()(stats_val v, int32_t x) noexcept {
  asio::post(stats_strand, [=, this]() {
    init_db_if_needed();

    db->write(influxdb::Point{module_id.data()} //
                  .addField("val", x)           //
                  .addTag("metric", val_txt[v]) //
                  .addTag("type", "counter"));  //
  });
}

void Stats::operator()(stats_val v, Elapsed &e) noexcept {

  // filter out normal durations
  // if ((v == NEXT_FRAME) && (e < 500us)) return;
  // if ((v == RENDER_DELAY) && (e < 1s)) return;

  asio::post(stats_strand, [=, this, count = e().count()]() {
    init_db_if_needed();

    db->write(influxdb::Point{module_id.data()} //
                  .addField("val", count)       //
                  .addTag("metric", val_txt[v]) //
                  .addTag("type", "duration")); //
  });
}

void Stats::operator()(stats_val v, const Micros d) noexcept {

  // filter out normal durations
  if ((v == REMOTE_ASYNC) && (d < 2ms)) return;
  if ((v == REMOTE_ELAPSED) && (d < InputInfo::lead_time())) return;
  if ((v == REMOTE_JITTER) && (d < InputInfo::lead_time())) return;
  if ((v == REMOTE_LONG_ROUNDTRIP) && (d < InputInfo::lead_time())) return;

  asio::post(stats_strand, [=, this]() {
    init_db_if_needed();

    db->write(influxdb::Point{module_id.data()} //
                  .addField("val", d.count())   //
                  .addTag("metric", val_txt[v]) //
                  .addTag("type", "duration")); //
  });
}

void Stats::operator()(stats_val v, const Nanos d) noexcept {

  // filter out normal durations
  // if ((v == SYNC_WAIT) && (d < InputInfo::lead_time())) return;

  asio::post(stats_strand, [=, this]() {
    init_db_if_needed();

    db->write(influxdb::Point{module_id.data()} //
                  .addField("val", d.count())   //
                  .addTag("metric", val_txt[v]) //
                  .addTag("type", "duration")); //
  });
}

} // namespace desk
} // namespace pierre