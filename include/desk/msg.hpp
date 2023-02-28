
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

#include "base/elapsed.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "lcs/logger.hpp"

#include <ArduinoJson.h>
#include <array>
#include <future>
#include <memory>
#include <vector>

namespace pierre {
namespace desk {

static constexpr size_t DOC_DEFAULT_MAX_SIZE{7 * 1024};
static constexpr size_t MSG_LEN_SIZE{sizeof(uint16_t)};
static constexpr size_t PACKED_DEFAULT_MAX_SIZE{DOC_DEFAULT_MAX_SIZE / 2};

typedef std::vector<char> Raw;
typedef std::vector<char> Packed;
using DynaDoc = DynamicJsonDocument;

static constexpr csv MAGIC{"magic"};
static constexpr uint16_t MAGIC_VAL{0xc9d2};
static constexpr csv NOW_US{"now_µs"};
static constexpr csv TYPE{"type"};

class Msg {
public:
  // outbound messages
  Msg(csv type, size_t max_size = DOC_DEFAULT_MAX_SIZE) noexcept
      : type(type.data(), type.size()), doc(max_size) {
    doc[TYPE] = type;
  }

  // inbound messages
  Msg(size_t max_size = DOC_DEFAULT_MAX_SIZE) : type("read"), doc(max_size) {}

  Msg(Msg &m) = delete;
  Msg(const Msg &m) = delete;
  Msg(Msg &&m) = default;

  void add_kv(csv key, auto val) noexcept {

    if constexpr (std::is_same_v<decltype(val), Millis>    //
                  || std::is_same_v<decltype(val), Micros> //
                  || std::is_same_v<decltype(val), Nanos>) //
    {
      doc[key] = val.count(); // convert durations
    } else {
      doc[key] = val;
    }
  }

  // for Msg RX
  auto buff_msg_len() noexcept { return asio::buffer(len_buff); }

  // for Msg RX
  auto buff_packed() noexcept {
    auto *p = reinterpret_cast<uint16_t *>(len_buff.data());
    packed_len = ntohs(*p);

    packed.assign(packed_len, 0x00); // we know the size of the incoming message

    return asio::buffer(packed.data(), packed.capacity());
  }

  // for Msg TX
  auto buff_seq() noexcept {
    uint16_t msg_len = htons(packed_len);
    memcpy(len_buff.data(), &msg_len, len_buff.size());

    // note: packed should be previously allocated by serialize

    return std::array{asio::buffer(len_buff), asio::buffer(packed)};
  }

  auto deserialize(size_t bytes) noexcept {
    const auto err = deserializeMsgPack(doc, packed.data(), packed.size());

    return !err && (bytes > 0);
  }

  auto elapsed() noexcept { return e.freeze(); }
  auto elapsed_restart() noexcept { return e.reset(); }

  auto key_equal(csv key, csv val) const noexcept { return val == doc[key]; }

  virtual void finalize() {} // override for additional work prior to serialization

  auto serialize() noexcept {
    finalize();

    doc[NOW_US] = pet::now_monotonic<Micros>().count();
    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    packed.assign(measureMsgPack(doc), 0x00);

    packed_len = serializeMsgPack(doc, packed.data(), packed.size());
  }

  bool xfer_error() const noexcept { return !xfer_ok(); }
  bool xfer_ok() const noexcept { return !ec && (xfr.bytes == (packed_len + MSG_LEN_SIZE)); }

  // misc logging, debug
  virtual string inspect() const noexcept;

  // error_code log_rx(const error_code ec, const size_t bytes, const char *err) noexcept;
  // error_code log_tx(const error_code ec, const size_t bytes) noexcept;

  // order dependent
  string type;
  DynaDoc doc;
  Raw len_buff{MSG_LEN_SIZE, 0x00};
  Packed packed;

  // order independent
  size_t packed_len{0};
  size_t tx_len{0};

  // async call result
  error_code ec;
  union {
    std::size_t in;
    std::size_t out;
    std::size_t bytes;
  } xfr{0};

  // duration tracking
private:
  Elapsed e;

public:
  static constexpr csv module_id{"io.msg.base"};
};

} // namespace desk
} // namespace pierre