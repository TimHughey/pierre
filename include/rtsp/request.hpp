//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "rtsp/headers.hpp"

#include <array>
#include <memory>

namespace pierre {
namespace rtsp {

class Request {

  // the magic number of 117 represents the minimum size RTSP message expected
  // ** plain-text only, not accounting for encryption **
  //
  // POST /feedback RTSP/1.0
  // CSeq: 15
  // DACP-ID: DF86B6D21A6C805F
  // Active-Remote: 1570223890
  // User-Agent: AirPlay/665.13.1

public:
  Request() = default;

  // prevent copies
  Request(Request &) = delete;
  Request(const Request &) = delete;
  Request &operator=(const Request &) = delete;
  Request &operator=(Request &) = delete;

  Request(Request &&) = default;            // allow move
  Request &operator=(Request &&) = default; // allow move

  auto buffer() noexcept {
    if (wire.size() && packet.empty()) e.reset();

    return asio::dynamic_buffer(wire);
  }

  auto find_delims() noexcept {
    delims = packet.find_delims(delims_want);

    return std::ssize(delims) == std::ssize(delims_want);
  }

  size_t populate_content() noexcept;

  void record_elapsed() noexcept;

public:
  Headers headers;
  uint8v content;
  uint8v packet; // always deciphered
  uint8v wire;   // maybe ciphered
  uint8v::delims_t delims;
  Elapsed e;

private:
  static constexpr csv CRLF{"\r\n"};
  static constexpr csv CRLFx2{"\r\n\r\n"};
  const std::array<csv, 2> delims_want{CRLF, CRLFx2};

public:
  static constexpr csv module_id{"rtsp::request"};
};

} // namespace rtsp
} // namespace pierre