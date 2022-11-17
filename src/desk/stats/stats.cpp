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

#include "desk/stats.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"

#include <InfluxDBFactory.h>
#include <memory>

namespace pierre {
namespace desk {

static std::unique_ptr<influxdb::InfluxDB> db;
static std::shared_ptr<Stats> self;

Stats::Stats(io_context &io_ctx, csv measure, const string db_uri) noexcept
    : db_uri(db_uri),       // where we save stats
      measurement(measure), // measurement
      stats_strand(io_ctx), // isolated strand for stats activities
      val_txt({             // create map of stats val to text
               {CLOCKS_DIFF, "clocks_diff"},
               {CTRL_CONNECT_ELAPSED, "ctrl_connect_elapsed"},
               {CTRL_CONNECT_TIMEOUT, "ctrl_connect_timeout"},
               {CTRL_MSG_READ_ELAPSED, "ctrl_msg_read_elapsed"},
               {CTRL_MSG_READ_ERROR, "ctrl_msg_read_error"},
               {CTRL_MSG_WRITE_ELAPSED, "ctrl_msg_write_elapsed"},
               {CTRL_MSG_WRITE_ERROR, "ctrl_msg_write_error"},
               {DATA_MSG_WRITE_ELAPSED, "data_msg_write_elapsed"},
               {DATA_MSG_WRITE_ERROR, "data_msg_write_error"},
               {FPS, "fps"},
               {FRAMES_RENDERED, "frames_rendered"},
               {FRAMES_SILENT, "frames_silent"},
               {FRAMES, "frames"},
               {FREQUENCY, "frequency"},
               {MAGNITUDE, "magnitude"},
               {NEXT_FRAME, "next_frame"},
               {NO_CONN, "no_conn"},
               {REELS_RACKED, "reels_racked"},
               {REMOTE_DATA_WAIT, "remote_data_wait"},
               {REMOTE_ELAPSED, "remote_elapsed"},
               {REMOTE_ROUNDTRIP, "remote_log_roundtrip"},
               {RENDER, "render"},
               {RENDER_DELAY, "render_delay"},
               {RENDER_ELAPSED, "render_elapsed"},
               {STREAMS_DEINIT, "streams_deinit"},
               {STREAMS_INIT, "streams_init"},
               {SYNC_WAIT, "sync_wait"}}) {}

Stats::~Stats() noexcept {
  if (db) db.reset();
}

std::shared_ptr<Stats> Stats::create(io_context &io_ctx, csv measure,
                                     const string db_uri) noexcept {
  if (self.use_count() == 0) {
    self = std::shared_ptr<Stats>(new Stats(io_ctx, measure, db_uri));
  }

  return self->shared_from_this();
}

std::shared_ptr<Stats> Stats::init() noexcept {
  INFO(module_id, "INIT", "db_uri={}\n", db_uri);

  db = influxdb::InfluxDBFactory::Get(db_uri);
  db->batchOf();

  return shared_from_this();
}

void Stats::shutdown() noexcept { self.reset(); }

template <class... Ts> struct overload : Ts... { using Ts::operator()...; };

void Stats::write_stat(stats_v vt, stat_variant sv, csv type) noexcept {
  asio::post(self->stats_strand, [=]() mutable {
    auto pt = influxdb::Point(module_id.data());

    std::visit(overload{[&](double v) mutable { pt.addField("val", v); }, //
                        [&](int64_t v) mutable { pt.addField("val", v); },
                        [&](int32_t v) mutable { pt.addField("val", v); }},
               sv);

    pt.addTag("metric", self->val_txt[vt]);
    pt.addTag("type", type);

    db->write(std::move(pt));
  });
}

} // namespace desk
} // namespace pierre