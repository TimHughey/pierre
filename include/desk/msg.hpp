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
#include "dsp/peak_info.hpp"
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
  Msg(const PeakInfo &pi)
      : root(doc.to<JsonObject>()), // grab a reference to the root object
        dmx_frame(64, 0x00),        // init the dmx frame to all zeros
        silence(pi.silence)         // is this silence?
  {
    root["type"] = "desk";
    root["seq_num"] = pi.seq_num;
    root["timestamp"] = pi.timestamp; // frame RTSP timestamp
    root["silence"] = silence;
    root["nettime_now_µs"] = pe_time::as_duration<Nanos, Micros>(pi.nettime_now).count();
    root["frame_localtime_µs"] = pe_time::as_duration<Nanos, Micros>(pi.frame_localtime).count();
    root["uptime_µs"] = pi.uptime.as<Micros>().count();
    root["now_µs"] = pe_time::now_epoch<Micros>().count();
  }

public:
  static shMsg create(const PeakInfo &pi) { return shMsg(new Msg(pi)); }
  shMsg ptr() { return shared_from_this(); }

  auto dmxFrame() { return dmx_frame.data(); }
  JsonObject &rootObj() { return root; }

  void transmit(udp_socket &socket, udp_endpoint &endpoint) {
    std::array<char, 1024> packed{0};

    // string serialize_debug;
    // [[maybe_unused]] auto json_len = serializeJsonPretty(doc, serialize_debug);
    // __LOGX(LCOL01 "json len={}\n{}\n", "desk::Msg", "TRANSMIT", json_len, serialize_debug);

    if (auto packed_len = serializeMsgPack(doc, packed.data(), packed.size()); packed_len > 0) {
      auto buff = asio::const_buffer(packed.data(), packed_len);

      [[maybe_unused]] error_code ec;
      [[maybe_unused]] auto tx_bytes = socket.send_to(buff, endpoint, 0, ec);
    }
  }

  void transmit2(strand &local_strand, tcp_socket &socket) {
    error_code ec;
    std::array<char, 1024> packed{0};

    auto dframe = root.createNestedArray("dframe");
    for (auto byte : dmx_frame) {
      dframe.add(byte);
    }

    // magic added to end of document as additional check of message completeness
    root["magic"] = 0xc9d2;

    __LOGX(LCOL01 " {}\n", "desk::Msg", "TRANSMIT2", inspect());

    if (auto packed_len = serializeMsgPack(doc, packed.data(), packed.size()); packed_len > 0) {
      __LOGX(LCOL01 " sending MsgPax len={}\n", "desk::Msg", "TRANSMIT2", packed_len);

      uint16_t header = htons(packed_len); // host to network order

      if (socket.is_open()) {
        auto buffers = std::array{asio::const_buffer(&header, sizeof(header)),
                                  asio::const_buffer(packed.data(), packed_len)};

        asio::async_write(
            socket,              // write to this socket
            buffers,             // send these buffers
            asio::bind_executor( // serialize
                local_strand, [](const error_code ec, [[maybe_unused]] size_t tx_bytes) {
                  if (ec) {
                    __LOGX(LCOL01 " async_write() failed tx_bytes={} reason={}\n", //
                           "desk::Msg", "TRANSMIT", tx_bytes, ec.message());
                  } else {
                    __LOGX(LCOL01, " async_write() tx_bytes={}\n", tx_bytes);
                  }
                }));
      }
    }
  }

  string inspect() const {
    string msg;

    fmt::format_to(std::back_inserter(msg), " silence={} json_len={} dmx_len={}\n", //
                   silence, measureJson(doc), dmx_frame.size());

    serializeJson(doc, msg);

    return msg;
  }

private:
  // order dependent
  StaticJsonDocument<DOC_SIZE> doc; // must match ruth
  JsonObject root;
  uint8v dmx_frame;
  bool silence;
};

} // namespace desk
} // namespace pierre
