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
#include "base/pet.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"

#include <ArduinoJson.h>
#include <arpa/inet.h>
#include <array>
#include <future>
#include <memory>
#include <vector>

namespace pierre {
namespace io {

static constexpr size_t DOC_DEFAULT_MAX_SIZE = 7 * 1024;
static constexpr size_t MSG_LEN_SIZE = sizeof(uint16_t);
static constexpr size_t PACKED_DEFAULT_MAX_SIZE = DOC_DEFAULT_MAX_SIZE / 2;

typedef std::vector<char> Raw;
typedef std::vector<char> Packed;
using DynaDoc = DynamicJsonDocument;

static constexpr csv MAGIC{"magic"};
static constexpr uint16_t MAGIC_VAL = 0xc9d2;
static constexpr csv NOW_US{"now_µs"};
static constexpr csv TYPE{"type"};

class Msg {
public:
  // outbound messages
  Msg(csv type, size_t max_size = DOC_DEFAULT_MAX_SIZE)
      : type(type.data(), type.size()), //
        doc(max_size)                   //
  {
    doc[TYPE] = type;
  }

  // inbound messages
  Msg(size_t max_size = DOC_DEFAULT_MAX_SIZE) : type("read"), doc(max_size) {}

  Msg(Msg &m) = delete;
  Msg(const Msg &m) = delete;
  Msg(Msg &&m) = default;

  void add_kv(csv key, auto val) { doc[key] = val; }

  // for Msg RX
  auto buff_msg_len() { return asio::buffer(len_buff.data(), len_buff.size()); }

  // for Msg RX
  auto buff_packed() {
    auto *p = reinterpret_cast<uint16_t *>(len_buff.data());
    packed_len = ntohs(*p);

    packed.reserve(packed_len); // we know the size of the incoming message

    return asio::buffer(packed.data(), packed.capacity());
  }

  // for Msg TX
  auto buff_seq() {
    uint16_t msg_len = htons(packed_len);
    memcpy(len_buff.data(), &msg_len, len_buff.size());

    tx_len = packed_len + len_buff.size();

    return std::array{const_buff(len_buff.data(), len_buff.size()), //
                      const_buff(packed.data(), packed.capacity())};
  }

  auto deserialize(error_code ec, size_t bytes) {
    const auto err = deserializeMsgPack(doc, packed.data(), packed.capacity());

    log_rx(ec, bytes, err);

    return !err;
  }

  auto key_equal(csv key, csv val) const { return val == doc[key]; }

  virtual void finalize() {} // override for additional work prior to serialization

  auto serialize() {
    finalize();

    doc[NOW_US] = pet::reference<Micros>().count();
    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    packed.reserve(PACKED_DEFAULT_MAX_SIZE);

    packed_len = serializeMsgPack(doc, packed.data(), packed.capacity());

    __LOGX(LCOL01 "{}\n", module_id, "INSPECT", inspect());
  }

  // misc logging, debug
  virtual string inspect() const {
    string msg;

    fmt::format_to(std::back_inserter(msg), " packed_len={}\n", //
                   measureMsgPack(doc));

    serializeJsonPretty(doc, msg);

    return msg;
  }

  error_code log_rx(const error_code ec, const size_t bytes, const auto err) {
    if (ec || (packed_len != bytes) || err) {
      __LOG0(LCOL01 " failed, bytes={}/{} reason={} deserialize={}\n", module_id, type, bytes,
             tx_len, ec.message(), err.c_str());
    }

    return ec;
  }

  error_code log_tx(const error_code ec, const size_t bytes) {
    if (ec || (tx_len != bytes)) {
      __LOG0(LCOL01 " failed, bytes={}/{} reason={}\n", module_id, type, bytes, tx_len,
             ec.message());
    }

    return ec;
  }

  // order dependent
  string type;
  DynaDoc doc;
  Raw len_buff{MSG_LEN_SIZE, 0x00};
  Packed packed;

  // order independent
  size_t packed_len = 0;
  size_t tx_len = 0;

  // misc debug
  static constexpr csv module_id{"MSG_BASE"};
};

} // namespace io
} // namespace pierre