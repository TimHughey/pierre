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
#include "io/io.hpp"

#include <ArduinoJson.h>
#include <arpa/inet.h>
#include <array>
#include <future>
#include <memory>
#include <vector>

namespace pierre {
namespace io {

class Msg {

public:
  Msg(csv type, size_t max_size = DOC_DEFAULT_MAX_SIZE)
      : type(type.data(), type.size()), //
        doc(max_size),                  //
        root(doc.to<JsonObject>())      //
  {
    root[TYPE] = type;
    root[NOW_US] = pet::now_epoch<Micros>().count();
  }

  virtual ~Msg() {}

  void add_kv(csv key, auto val) { root[key] = val; }

  auto buff_seq() {
    finalize();

    root[MAGIC] = MAGIC_VAL;

    packed_len = serializeMsgPack(doc, packed.data(), packed.size());

    if (packed_len > 0) {
      tx_len = packed_len + MSG_LEN_SIZE;

      msg_len = htons(packed_len);
      return std::array{const_buff(&msg_len, MSG_LEN_SIZE), //
                        const_buff(packed.data(), packed_len)};
    }

    return std::array{const_buff(fail_buff.data(), fail_buff.size()),
                      const_buff(fail_buff.data(), fail_buff.size())};
  }

  // misc logging, debug
  error_code log_tx(const error_code ec, size_t bytes) {
    if (ec || (tx_len != bytes)) {
      __LOG0(LCOL01 " failed, bytes={}/{} reason={}\n", module_id, type, bytes, tx_len,
             ec.message());
    }

    return ec;
  }

  virtual void finalize() {}

  // order dependent
  string type;
  DynamicJsonDocument doc;
  JsonObject root;

  // order independent
  Packed packed;
  uint16_t msg_len = 0;
  size_t packed_len = 0;
  size_t tx_len = 0;

  // failed buff
  std::array<char, 1> fail_buff;

  // object key constants
  static constexpr uint16_t MAGIC_VAL = 0xc9d2;
  static constexpr csv MAGIC{"magic"};
  static constexpr csv NOW_US{"now_µs"};
  static constexpr csv TYPE{"type"};

  // misc debug
  static constexpr csv module_id{"MSG_BASE"};
};

} // namespace io
} // namespace pierre