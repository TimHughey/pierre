
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
#include "lcs/logger.hpp"

#include <ArduinoJson.h>
#include <memory>

namespace pierre {
namespace desk {

class MsgIn : public Msg {
public:
  // outbound messages
  MsgIn() = default;
  ~MsgIn() noexcept {} // prevent default copy/move

  MsgIn(MsgIn &&) = default;
  MsgIn &operator=(MsgIn &&) = default;

  bool calc_packed_len(io::streambuf &storage, std::size_t n = 0) noexcept {
    static constexpr csv fn_id{"calc_packed"};

    bool complete{false};
    auto hdr_buffer = storage.data();

    // simulate n from async_read when called by the constructor (n == 0)
    xfr.in += (n == 0) ? hdr_buffer.size() : n;

    if ((packed_len == 0) && (hdr_buffer.size() >= hdr_bytes)) {
      memcpy(&packed_len, hdr_buffer.data(), hdr_bytes);

      packed_len = ntohs(packed_len); // convert to host byte order

      // we leave the header bytes in the stream buffer until
      // deserialization so future creations of MsgIn can
      // assess if this is a complete message when the stream_bufffer
      // already contains data
    }

    // now check if the buffer contains packed data
    if (auto rest_size = storage.size(); rest_size > hdr_bytes) {
      complete = (rest_size - hdr_bytes) >= packed_len;

      if (!complete) need_bytes = packed_len - rest_size - hdr_bytes;
    }

    // INFO_AUTO("n={} packed_len={} complete={}\n", n, packed_len, complete);

    return complete; // invert to signal more bytes required
  }

  auto deserialize_from(auto &storage, JsonDocument &doc) noexcept {
    static constexpr csv fn_id{"deserialize"};

    // asio allows buffer math, add hdr_bytes so we get back
    // a buffer that points to the packed data
    auto buffer = storage.data() + hdr_bytes;

    auto raw = static_cast<const char *>(buffer.data());

    const auto err = deserializeMsgPack(doc, raw, packed_len);
    storage.consume(hdr_bytes + packed_len);

    if (err) {
      INFO_AUTO("deserialize err={}\n", err.c_str());
    }

    return !err;
  }

  auto read_bytes() noexcept { return asio::transfer_at_least(need_bytes); }

public:
  size_t need_bytes{0};

public:
  static constexpr csv module_id{"desk.msgin"};
};

} // namespace desk
} // namespace pierre