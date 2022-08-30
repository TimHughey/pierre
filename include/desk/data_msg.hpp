/*
    Pierre
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "base/elapsed.hpp"
#include "base/pe_time.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "frame/frame.hpp"
#include "io/msg.hpp"

namespace pierre {
namespace desk {

class DataMsg : public io::Msg {

public:
  DataMsg(shFrame frame)
      : Msg(TYPE),                // init base class
        dmx_frame(16, 0x00),      // init the dmx frame to all zeros
        silence(frame->silence()) // is this silence?
  {
    add_kv("seq_num", frame->seq_num);
    add_kv("timestamp", frame->timestamp); // RTSP timestamp
    add_kv("silence", silence);
    add_kv("nettime_now_µs", frame->nettime<Micros>().count());
    add_kv("frame_localtime_µs", frame->sync_wait_elapsed<Micros>().count());
    add_kv("sync_wait_µs", pet::as_duration<Nanos, Micros>(frame->sync_wait).count());
    add_kv("now_µs", pet::reference<Micros>().count());
  }

  DataMsg(Msg &m) = delete;
  DataMsg(const Msg &m) = delete;
  DataMsg(DataMsg &&m) = default;

public:
  uint8_t *dmxFrame() { return dmx_frame.data(); }

  void finalize() override {
    auto dframe = doc.createNestedArray("dframe");
    for (uint8_t byte : dmx_frame) {
      dframe.add(byte);
    }
  }

  // misc debug
  string inspect() const {
    string msg;

    fmt::format_to(std::back_inserter(msg), " silence={} packed_len={} dmx_len={}\n", //
                   silence, measureMsgPack(doc), dmx_frame.size());

    serializeJsonPretty(doc, msg);

    return msg;
  }

private:
  // order dependent
  static constexpr csv TYPE{"data"};
  uint8v dmx_frame;
  bool silence;
  static constexpr csv module_id{"DATA_MSG"};
};

} // namespace desk
} // namespace pierre
