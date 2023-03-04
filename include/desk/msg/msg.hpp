
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
#include "desk/msg/kv.hpp"
#include "io/buffer.hpp"
#include "io/error.hpp"

#include <ArduinoJson.h>
#include <functional>

namespace pierre {
namespace desk {

class Msg {

public:
  Msg(io::streambuf &buffer) noexcept : stream_buffer(buffer) {}

  virtual ~Msg() noexcept {} // prevent implicit copy/move

  Msg(Msg &&m) = default;
  Msg &operator=(Msg &&msg) = default;

  auto &buffer() noexcept { return stream_buffer.get(); }

  auto elapsed() noexcept { return e.freeze(); }
  auto elapsed_restart() noexcept { return e.reset(); }

  bool is_msg_type(const JsonDocument &doc, csv want_type) const noexcept {
    return want_type == csv{doc[MSG_TYPE].as<const char *>()};
  }

  bool xfer_error() const noexcept { return !xfer_ok(); }
  bool xfer_ok() const noexcept { return !ec && (xfr.bytes >= (packed_len + hdr_bytes)); }

public:
  // order dependent
  std::reference_wrapper<io::streambuf> stream_buffer;

  // order independent
  uint16_t packed_len{0};

  // async call result
  error_code ec;

  union {
    size_t in;
    size_t out;
    size_t bytes;
  } xfr{0};

  // duration tracking
private:
  Elapsed e;

public:
  static constexpr size_t default_doc_size{7 * 1024};
  static constexpr size_t hdr_bytes{sizeof(packed_len)};
  static constexpr csv module_id{"desk.msg"};
};

} // namespace desk
} // namespace pierre