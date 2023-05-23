
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
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/msg/kv.hpp"

#include <ArduinoJson.h>
#include <boost/asio/streambuf.hpp>
#include <boost/system/error_code.hpp>
#include <memory>

namespace pierre {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

namespace desk {

/// @brief Base class for Desk messages (in / out)
class Msg {
public:
  static constexpr size_t default_doc_size{7 * 1024};

public:
  /// @brief [base class] Construct the Desk message base
  /// @param capacity total capacity of the streambuf
  Msg(std::size_t capacity) noexcept : storage(std::make_unique<asio::streambuf>(capacity)) {}
  virtual ~Msg() noexcept {} // prevent implicit copy/move

  Msg(Msg &&m) = default;              // allow move construct
  Msg &operator=(Msg &&msg) = default; // allow move assignment

  /// @brief Provide the buffer used by async calls
  /// @return Reference to streambuf
  auto &buffer() noexcept { return *storage; }

  /// @brief Freeze the elapsed time and return the current value
  /// @return Elapsed time since the creation of the message
  auto elapsed() noexcept { return e.freeze(); }

  /// @brief Check the (subclassed) message type
  /// @param doc JsonDocument containing the message
  /// @param want_type Type of message to confirm
  /// @return boolean indicating if this message is the same type
  static bool is_msg_type(const JsonDocument &doc, csv want_type) noexcept {
    return want_type == csv{doc[MSG_TYPE].as<const char *>()};
  }

  /// @brief Was there an error in the transfer?
  /// @return boolean
  bool xfer_error() const noexcept { return !xfer_ok(); }

  /// @brief Was the transfer successful?
  /// @return boolean
  bool xfer_ok() const noexcept { return !ec && (xfr.bytes >= packed_len); }

protected:
  // order dependent
  std::unique_ptr<asio::streambuf> storage;

public:
  // order independent
  uint16_t packed_len{0};
  error_code ec;

  union {
    size_t in;
    size_t out;
    size_t bytes;
  } xfr{0};

protected:
  Elapsed e; // duration tracking

public:
  MOD_ID("desk.msg");
};

} // namespace desk
} // namespace pierre