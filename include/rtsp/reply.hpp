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
#include "rtsp/resp_code.hpp"

#include <algorithm>
#include <iterator>

namespace pierre {

namespace rtsp {

// forward decl to hide implementation details
class Ctx;

class Reply {

  // Building the response:
  // 1. Include CSeq header from request
  // 2. Include Server header
  // 3. Add Content
  // 4. Write data to socket

  // Payload Format:
  //
  // RTSP/1.0 200 OK\r\n
  // CSeq: <from request>
  // Content-Type: <based on content>
  // Header1: Value1\r\n
  // Header2: Value2\r\n
  // <more headers each followed by \r\n>
  // Content-Length: <val>  <<-- if there is content
  // \r\n  <<-- separate headers from content
  // <binary or plist content>

public:
  Reply() = default;

  // prevent copy
  Reply(Reply &) = delete;
  Reply(const Reply &) = delete;
  Reply &operator=(const Reply &) = delete;
  Reply &operator=(Reply &) = delete;

  // allow move
  Reply(Reply &&) = default;
  Reply &operator=(Reply &&) = default;

  void build(Ctx *ctx, const Headers &headers_in, const uint8v &content_in) noexcept;

  auto buffer() const noexcept { return asio::buffer(wire); }

  void copy_to_content(const uint8_t *begin, const size_t bytes) noexcept {
    auto w = std::back_inserter(content_out);
    std::copy_n(begin, bytes, w);
  }

  void copy_to_content(const auto &buf) noexcept {
    auto w = std::back_inserter(content_out);
    std::copy_n(buf.begin(), std::size(buf), w);
  }

  bool empty() const noexcept { return std::empty(wire); }

  bool has_content() noexcept { return !content_out.empty(); }

  void operator()(RespCode::code_val val) noexcept { resp_code(val); }

  void set_resp_code(RespCode::code_val val) noexcept { resp_code(val); }

public:
  // order independent
  Headers headers_out;
  uint8v content_out;
  RespCode resp_code{RespCode::NotImplemented};
  string error;
  uint8v wire;

public:
  static constexpr csv module_id{"rtsp.reply"};
};

} // namespace rtsp
} // namespace pierre