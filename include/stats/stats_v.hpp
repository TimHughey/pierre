
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

namespace pierre {
namespace stats {

enum stats_v {
  CLOCK_DIFF,
  CTRL_CONNECT_ELAPSED,
  CTRL_CONNECT_TIMEOUT,
  CTRL_MSG_READ_ELAPSED,
  CTRL_MSG_READ_ERROR,
  CTRL_MSG_WRITE_ELAPSED,
  CTRL_MSG_WRITE_ERROR,
  DATA_CONNECT_ELAPSED,
  DATA_CONNECT_FAILED,
  DATA_MSG_WRITE_ELAPSED,
  DATA_MSG_WRITE_ERROR,
  FLUSH_ELAPSED,
  FPS,
  FRAMES_OUTDATED,
  FRAMES_RENDERED,
  FRAMES_SILENT,
  MAX_PEAK_FREQUENCY,
  MAX_PEAK_MAGNITUDE,
  NEXT_FRAME_WAIT,
  NO_CONN,
  RACK_COLLISION,
  RACK_WIP_INCOMPLETE,
  RACK_WIP_TIMEOUT,
  RACKED_REELS,
  REELS_FLUSHED,
  REMOTE_DATA_WAIT,
  REMOTE_DMX_QOK,
  REMOTE_DMX_QRF,
  REMOTE_DMX_QSF,
  REMOTE_ELAPSED,
  REMOTE_ROUNDTRIP,
  RENDER_DELAY,
  RENDER_ELAPSED,
  RTSP_SESSION_CONNECT,
  RTSP_SESSION_RX_AVAIL,
  RTSP_SESSION_RX_FIRST,
  SYNC_WAIT,
  // extra comma allows for easy IDE sorting
};

} // namespace stats
} // namespace pierre
