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

#include "base/stats/vals.hpp"
#include "base/types.hpp"

#include <map>

namespace pierre {
namespace stats {

/// @brief Creates the stats val to human readable timeseries db metric
///        this free function is only called by Stats
/// @return map of val to text
inline std::map<stats::stats_v, string> make_map() noexcept {

  return std::map<stats::stats_v, string>{
      // create map of stats val to text
      {stats::FRAMES_AVAIL, "frames_avail"},
      {stats::DATA_CONNECT_ELAPSED, "data_connect_elapsed"},
      {stats::DATA_MSG_READ_ELAPSED, "data_msg_read_elapsed"},
      {stats::DATA_MSG_READ_ERROR, "data_msg_read_error"},
      {stats::DATA_MSG_WRITE_ELAPSED, "data_msg_write_elapsed"},
      {stats::DATA_MSG_WRITE_ERROR, "data_msg_write_error"},
      {stats::FLUSH_ELAPSED, "flush_elapsed"},
      {stats::FPS, "fps"},
      {stats::FRAME, "frame"},
      {stats::NEXT_FRAME_WAIT, "next_frame_wait"},
      {stats::PEAK, "peak"},
      {stats::REMOTE_DATA_WAIT, "remote_data_wait"},
      {stats::REMOTE_DMX_QOK, "remote_dmx_qok"},
      {stats::REMOTE_DMX_QRF, "remote_dmx_qrf"},
      {stats::REMOTE_DMX_QSF, "remote_dmx_qsf"},
      {stats::REMOTE_ELAPSED, "remote_elapsed"},
      {stats::REMOTE_ROUNDTRIP, "remote_roundtrip"},
      {stats::RENDER_ELAPSED, "render_elapsed"},
      {stats::RTSP_AUDIO_CIPHERED, "rtsp_audio_ciphered"},
      {stats::RTSP_AUDIO_DECIPERED, "rtsp_audio_deciphered"},
      {stats::RTSP_SESSION_CONNECT, "rtsp_session_connect"},
      {stats::RTSP_SESSION_MSG_ELAPSED, "rtsp_session_msg_elapsed"},
      {stats::RTSP_SESSION_RX_PACKET, "rtsp_session_rx_packet"},
      {stats::RTSP_SESSION_TX_REPLY, "rtsp_session_tx_reply"},
      {stats::SYNC_WAIT, "sync_wait"},
      // comment allows for easy IDE sorting
  };
}

} // namespace stats
} // namespace pierre