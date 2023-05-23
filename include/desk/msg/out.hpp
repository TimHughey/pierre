
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

#include "base/clock_now.hpp"
#include "desk/msg/kv.hpp"
#include "desk/msg/msg.hpp"

#include <ArduinoJson.h>
#include <boost/system/errc.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <ranges>
#include <variant>
#include <vector>

namespace pierre {

namespace errc = boost::system::errc;

namespace ranges = std::ranges;

namespace desk {

/// @brief Desk Out Msg [subclass of Msg]
class MsgOut : public Msg {
  // note: these are the only types that Units set
  using val_t = std::variant<uint16_t, uint32_t, float, bool, int64_t, string>;

  struct kve {
    string key;
    val_t val;
  };

public:
  enum msg_t : uint8_t { Err = 0 };

protected:
  /// @brief Provide direct access to the buffer for subclasses
  /// @param buffer Buffer to provide access to
  /// @return Raw char pointer to the buffer
  auto *raw_out(auto &buffer) noexcept { return static_cast<char *>(buffer.data()); }

  /// @brief Provide a final opportunity for subclasses to augment the JsonDocument
  /// @param JsonDocument
  virtual void serialize_hook(JsonDocument &) noexcept {}

public:
  /// @brief Desk message (out) [subclass of Msg]
  /// @param type type of message
  MsgOut(auto &type) noexcept : Msg(2048), type(type) {
    // msg type must be the first key/val in the document
    add_kv(desk::MSG_TYPE, type);
  }

  virtual ~MsgOut() noexcept {} // prevent implicit copy/move

  MsgOut(MsgOut &&m) = default;
  MsgOut &operator=(MsgOut &&) = default;

  /// @brief Submit a key/value for inclusion in outbound msg
  /// @param key string
  /// @param val Duration, uint16_t, uint32_t, float, bool, int64_t or string
  void add_kv(const auto &key, auto &&val) noexcept {
    using val_t = decltype(val);

    if constexpr (IsDuration<val_t>) {
      key_vals.emplace_back(kve{key, val.count()});
    } else {
      key_vals.emplace_back(kve{key, std::move(val)});
    }
  }

  /// @brief Commit bytes received to the streambuf
  /// @param n bytes
  void commit(std::size_t n) noexcept { storage->commit(n); }

  /// @brief Functor for async_write
  /// @param op_ec Error code from async_write
  /// @param n Number of bytes written
  void operator()(const error_code &op_ec, std::size_t n) noexcept {
    ec = op_ec;
    xfr.out = n;

    if (!n && (ec.value() != errc::operation_canceled)) {
      status_msgs[Err] = fmt::format("SHORT WRITE  n={} err={}", xfr.out, ec.message());
    }
  }

  /// @brief Provide buffer for the data to send
  /// @return Buffer
  auto prepare() noexcept { return storage->prepare(storage->max_size()); }

  /// @brief Prepare msg for transmit
  /// @return Number of bytes committed to the buffer
  auto serialize() noexcept {
    StaticJsonDocument<Msg::default_doc_size> doc;

    // put added key/vals into the document by visiting each
    // key/val direcrlt setting them in the document
    for (auto &e : key_vals) {
      std::visit([&](auto &&v) { doc[e.key] = v; }, e.val);
    }

    // allow subclasses to add special data directly to the doc
    serialize_hook(doc);

    // finally, add the trailer
    doc[NOW_US] = clock_now::mono::us();
    doc[NOW_REAL_US] = clock_now::real::us();
    doc[MAGIC] = MAGIC_VAL; // add magic as final key (to confirm complete msg)

    auto buffer = prepare();

    packed_len = serializeMsgPack(doc, raw_out(buffer), buffer.size());

    // INFO_AUTO("{}", fmt::streamed(doc));

    commit(packed_len);
  }

  /// @brief Get status meesage
  /// @param msg_kind Kind of message
  /// @return const string reference
  auto const &status(auto msg_kind) const noexcept { return status_msgs[msg_kind]; }

public:
  // order dependent
  string type;

  // order independent
  std::vector<kve> key_vals;
  std::array<string, 2> status_msgs;

public:
  MOD_ID("desk.msg.out");
};

} // namespace desk
} // namespace pierre