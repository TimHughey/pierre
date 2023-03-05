
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

#include "desk/msg/kv.hpp"
#include "desk/msg/kv_store.hpp"
#include "desk/msg/msg.hpp"

#include "arpa/inet.h"
#include <ArduinoJson.h>

namespace pierre {
namespace desk {

class MsgOut : public Msg {

public:
  // outbound messages
  MsgOut(auto &type) noexcept : Msg(), type(type) {}

  // inbound messages
  virtual ~MsgOut() noexcept {} // prevent implicit copy/move

  MsgOut(MsgOut &&m) = default;
  MsgOut &operator=(MsgOut &&) = default;

  template <typename Val> void add_kv(auto &&key, Val &&val) noexcept {
    if constexpr (IsDuration<Val>) {
      kvs.add(std::forward<decltype(key)>(key), val.count());
    } else {
      kvs.add(std::forward<decltype(key)>(key), std::forward<Val>(val));
    }
  }

  auto serialize_to(auto &storage) noexcept {
    DynamicJsonDocument doc(Msg::default_doc_size);

    // first, add MSG_TYPE as it is used to detect start of message
    doc[desk::MSG_TYPE] = type;

    // allow subclasses to add special data directly to the doc
    serialize_hook(doc);

    // put added key/vals into the document
    kvs.populate_doc(doc);

    // finally, add the trailer
    doc[NOW_US] = clock_now::mono::us();
    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    packed_len = measureMsgPack(doc);

    auto buffer1 = storage.prepare(packed_len + hdr_bytes);

    net_packed_len = htons(packed_len);
    memcpy(buffer1.data(), &net_packed_len, hdr_bytes);

    auto buffer2 = buffer1 + hdr_bytes;

    serializeMsgPack(doc, static_cast<char *>(buffer2.data()), buffer2.size());
    storage.commit(packed_len + hdr_bytes);
  }

protected:
  virtual void serialize_hook(JsonDocument &) noexcept {}

public:
  // order dependent
  string type;

  // order independent
  kv_store kvs;
  uint16_t net_packed_len;

public:
  static constexpr csv module_id{"desk.msg.out"};
};

} // namespace desk
} // namespace pierre