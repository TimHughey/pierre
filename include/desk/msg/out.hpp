
// Pierre
// Copyright (C) 2022  Tim Hughey
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

#pragma once

#include "desk/msg/msg.hpp"

#include <ArduinoJson.h>

namespace pierre {
namespace desk {

class MsgOut : public Msg {
public:
  // outbound messages
  MsgOut(io::streambuf &buffer, auto &&type) noexcept
      : Msg(buffer), doc(Msg::default_doc_size), type(type) {}

  // inbound messages
  virtual ~MsgOut() noexcept {} // prevent implicit copy/move

  MsgOut(MsgOut &&m) = default;
  MsgOut &operator=(MsgOut &&) = default;

  void add_kv(csv key, auto val) noexcept {
    if constexpr (IsDuration<decltype(val)>) {
      doc[key] = val.count(); // convert durations
    } else {
      doc[key] = val;
    }
  }

  auto serialize() noexcept {

    doc[MSG_TYPE] = type;
    doc[NOW_US] = clock_now::mono::us();
    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    packed_len = measureMsgPack(doc);

    auto buffer1 = stream_buffer.get().prepare(packed_len + hdr_bytes);

    net_packed_len = htons(packed_len);
    memcpy(buffer1.data(), &net_packed_len, hdr_bytes);

    auto buffer2 = buffer1 + hdr_bytes;

    serializeMsgPack(doc, static_cast<char *>(buffer2.data()), buffer2.size());
    stream_buffer.get().commit(packed_len + hdr_bytes);
  }

public:
  // order dependent
  DynamicJsonDocument doc;
  string type;

  // order independent
  uint16_t net_packed_len;

public:
  static constexpr csv module_id{"desk.msg.out"};
};

} // namespace desk
} // namespace pierre