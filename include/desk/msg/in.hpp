
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
#include <algorithm>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <ranges>
#include <vector>

namespace pierre {
namespace desk {

class MsgIn : public Msg {

protected:
  /// @brief Provide direct access to the buffer for subclasses
  /// @param buffer Buffer to provide access to
  /// @return Raw char pointer to the buffer
  auto *raw_in() noexcept { return static_cast<const char *>(storage->data().data()); }

public:
  /// @brief Desk message (in) [subclass of Msg]
  MsgIn() : Msg(512) {}
  ~MsgIn() noexcept {}

  MsgIn(MsgIn &&) = default;
  MsgIn &operator=(MsgIn &&) = default;

  /// @brief Functor for async_read
  /// @param op_ec Error code from async_read
  /// @param n Number of bytes read
  void operator()(const error_code &op_ec, size_t n) noexcept {
    static constexpr csv fn_id{"async_result"};

    xfr.in += n;
    ec = op_ec;
    packed_len = n; // should we need to set this?

    if (!n && (ec.value() != errc::operation_canceled)) {
      status_msgs[Err] = fmt::format("SHORT READ  n={} err={}", xfr.in, ec.message());
    } else {
      status_msgs[Err].clear();
    }
  }

  /// @brief Consume bytes from the streambuf
  /// @param n num bytes to consume
  void consume(std::size_t n) noexcept { storage->consume(n); }

  /// @brief Deserialize the availablt streambuf data
  /// @param doc JsonDocument reference
  /// @return boolean indicating success or failure
  auto deserialize_into(JsonDocument &doc) noexcept {
    static constexpr csv fn_id{"deserialize"};
    const auto err = deserializeMsgPack(doc, raw_in(), xfr.in);
    consume(xfr.in);

    if (err) INFO_AUTO("deserialize err={}\n", err.c_str());

    return !err;
  }

  /// @brief Bytes committed (read) into the streambuf
  /// @return
  auto in_avail() noexcept { return storage->in_avail(); }

  /// @brief Clear msg and prepare it to be reused
  void reuse() noexcept {
    packed_len = 0;
    ec = error_code();
    xfr.bytes = 0;
    e.reset();
  }

public:
  MOD_ID("desk.msg.in");
};

} // namespace desk
} // namespace pierre