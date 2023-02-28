// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "dmx_data_msg.hpp"
#include "base/input_info.hpp"
#include "base/pet.hpp"

namespace pierre {

DmxDataMsg::DmxDataMsg(frame_t frame) noexcept
    : Msg(TYPE),               // init base class
      dmx_frame(16, 0x00),     // init the dmx frame to all zeros
      silence(frame->silent()) // is this silence?
{
  add_kv("seq_num", frame->seq_num);
  add_kv("timestamp", frame->timestamp); // RTSP timestamp
  add_kv("silence", silence);
  add_kv("lead_time_µs", InputInfo::lead_time_us);
  add_kv("sync_wait_µs", pet::as<Micros>(frame->sync_wait()).count());
}

} // namespace pierre