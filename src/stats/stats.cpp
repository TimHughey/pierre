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

#include "stats/stats.hpp"
#include "base/logger.hpp"
#include "config/config.hpp"

#include <latch>
#include <memory>

namespace pierre {
namespace stats {
static std::shared_ptr<Stats> self;
}

Stats::Stats(bool enabled) noexcept
    : guard(io::make_work_guard(io_ctx)), //
      enabled(enabled), val_txt{
                            // create map of stats val to text
                            {stats::CTRL_CONNECT_ELAPSED, "ctrl_connect_elapsed"},
                            {stats::CTRL_CONNECT_TIMEOUT, "ctrl_connect_timeout"},
                            {stats::CTRL_MSG_READ_ELAPSED, "ctrl_msg_read_elapsed"},
                            {stats::CTRL_MSG_READ_ERROR, "ctrl_msg_read_error"},
                            {stats::CTRL_MSG_WRITE_ELAPSED, "ctrl_msg_write_elapsed"},
                            {stats::CTRL_MSG_WRITE_ERROR, "ctrl_msg_write_error"},
                            {stats::DATA_CONNECT_ELAPSED, "data_connect_elapsed"},
                            {stats::DATA_CONNECT_FAILED, "data_connect_failed"},
                            {stats::DATA_MSG_WRITE_ELAPSED, "data_msg_write_elapsed"},
                            {stats::DATA_MSG_WRITE_ERROR, "data_msg_write_error"},
                            {stats::FLUSH_ELAPSED, "flush_elapsed"},
                            {stats::FPS, "fps"},
                            {stats::FRAME, "frame"},
                            {stats::MAX_PEAK_FREQUENCY, "max_peak_frequency"},
                            {stats::MAX_PEAK_MAGNITUDE, "max_peak_magnitude"},
                            {stats::NEXT_FRAME_WAIT, "next_frame_wait"},
                            {stats::NO_CONN, "no_conn"},
                            {stats::RACK_COLLISION, "rack_collision"},
                            {stats::RACK_WIP_INCOMPLETE, "rack_wip_incomplete"},
                            {stats::RACK_WIP_TIMEOUT, "rack_wip_timeout"},
                            {stats::RACKED_REELS, "racked_reels"},
                            {stats::REELS_FLUSHED, "reels_flushed"},
                            {stats::REMOTE_DATA_WAIT, "remote_data_wait"},
                            {stats::REMOTE_DMX_QOK, "remote_dmx_qok"},
                            {stats::REMOTE_DMX_QRF, "remote_dmx_qrf"},
                            {stats::REMOTE_DMX_QSF, "remote_dmx_qsf"},
                            {stats::REMOTE_ELAPSED, "remote_elapsed"},
                            {stats::REMOTE_ROUNDTRIP, "remote_roundtrip"},
                            {stats::RENDER_DELAY, "render_delay"},
                            {stats::RENDER_ELAPSED, "render_elapsed"},
                            {stats::RTSP_AUDIO_CIPHERED, "rtsp_audio_ciphered"},
                            {stats::RTSP_AUDIO_DECIPERED, "rtsp_audio_deciphered"},
                            {stats::RTSP_SESSION_CONNECT, "rtsp_session_connect"},
                            {stats::RTSP_SESSION_MSG_ELAPSED, "rtsp_session_msg_elapsed"},
                            {stats::RTSP_SESSION_RX_PACKET, "rtsp_session_rx_packet"},
                            {stats::RTSP_SESSION_TX_REPLY, "rtsp_session_tx_reply"},
                            {stats::SYNC_WAIT, "sync_wait"},
                            // comment allows for easy IDE sorting
                        } {}

void Stats::init() noexcept {
  if (!stats::self) {
    const auto db_uri = Config().at("stats.db_uri"sv).value_or(string());
    auto enabled = Config().at("stats.enabled"sv).value_or(false);

    if (db_uri.size() && enabled) {

      stats::self = std::shared_ptr<Stats>(new Stats(enabled));
      stats::self->init_self(db_uri);

      INFO(module_id, "INIT", "sizeof={} db_uri={} enabled={}\n", sizeof(Stats), db_uri, enabled);

    } else {
      INFO(module_id, "INIT", "stats are disabled\n");
    }
  }
}

void Stats::init_self(const string &db_uri) noexcept {

  if (enabled) {
    db = influxdb::InfluxDBFactory::Get(db_uri);

    if (const std::size_t bs = Config().at("stats.batch_of"sv).value_or(0); bs > 0) {
      db->batchOf(bs);
    }

    std::latch latch(1);

    threads.emplace_back([s = shared_from_this(), &latch](std::stop_token token) mutable {
      s->tokens.add(std::move(token));

      name_thread("Stats", 0);

      // we want all airplay threads to start at once
      latch.count_down();
      s->io_ctx.run();
    });

    latch.wait(); // wait for all threads to start
  }
}

// DRAGONS BE HERE!!
// returns reference to ACTUAL shared_ptr holding Stats
std::shared_ptr<Stats> &Stats::self() noexcept { return stats::self; }

void Stats::shutdown([[maybe_unused]] io_context &io_ctx) noexcept { self().reset(); }

} // namespace pierre
