/*
    Pierre - Custom Light Show via Msg for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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
#include "io/io.hpp"

#include <ArduinoJson.h>
#include <arpa/inet.h>
#include <array>
#include <fmt/core.h>
#include <memory>
#include <vector>

namespace pierre {
namespace desk {

class Msg;
typedef std::shared_ptr<Msg> shMsg;

class Msg : public std::enable_shared_from_this<Msg> {
private:
  static constexpr size_t DOC_SIZE = 4096; // JSON_ARRAY_SIZE(64) + JSON_OBJECT_SIZE(13);

private:
  Msg(shFrame frame)
      : root(doc.to<JsonObject>()), // grab a reference to the root object
        dmx_frame(16, 0x00),        // init the dmx frame to all zeros
        silence(frame->silence())   // is this silence?
  {
    root["type"] = "desk";
    root["seq_num"] = frame->seq_num;
    root["timestamp"] = frame->timestamp; // frame RTSP timestamp
    root["silence"] = silence;
    root["nettime_now_µs"] = frame->nettime<Micros>().count();
    root["frame_localtime_µs"] = frame->sync_wait_elapsed<Micros>().count();
    root["sync_wait_µs"] = pe_time::as_duration<Nanos, Micros>(frame->sync_wait).count();
  }

public:
  static shMsg create(shFrame frame) { return shMsg(new Msg(frame)); }
  shMsg ptr() { return shared_from_this(); }

  auto dmxFrame() { return dmx_frame.data(); }
  JsonObject &rootObj() { return root; }

  inline void transmit(udp_socket &socket, udp_endpoint &endpoint) {
    auto dframe = root.createNestedArray("dframe");
    for (auto byte : dmx_frame) {
      dframe.add(byte);
    }

    // magic added to end of document as additional check of message completeness
    root["magic"] = 0xc9d2;
    root["now_µs"] = pe_time::now_epoch<Micros>().count();

    __LOGX(LCOL01 " {}\n", "desk::Msg", "TRANSMIT", inspect());

    if (auto packed_len = serializeMsgPack(doc, packed.data(), packed.size()); packed_len > 0) {
      if (socket.is_open()) {

        auto buff = asio::const_buffer(packed.data(), packed_len);

        error_code ec;
        if (auto bytes = socket.send_to(buff, endpoint, 0, ec); ec) {
          __LOG0(LCOL01 " failed, dest={}:{} bytes={} reason={}\n", "desk::Msg", "SEND_TO",
                 endpoint.address().to_string(), endpoint.port(), bytes, ec.message());
        }
      }
    }
  }

  string inspect() const {
    string msg;

    fmt::format_to(std::back_inserter(msg), " silence={} packed_len={} dmx_len={}\n", //
                   silence, measureMsgPack(doc), dmx_frame.size());

    serializeJsonPretty(doc, msg);

    return msg;
  }

private:
  // order dependent
  StaticJsonDocument<DOC_SIZE> doc; // must match ruth
  JsonObject root;
  uint8v dmx_frame;
  bool silence;

  // order independent
  std::array<uint8_t, 1024> packed;
};

} // namespace desk
} // namespace pierre
