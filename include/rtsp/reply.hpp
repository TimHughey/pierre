/*
    Pierre - Custom Light Show for Wiss Landing
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

#include <array>
#include <cctype>
#include <fmt/core.h>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "rtsp/headers.hpp"
#include "rtsp/request.hpp"

namespace pierre {
namespace rtsp {

using std::string, std::string_view, std::tuple, std::unordered_map, std::vector;

// Building the response
// 1. Include CSeq header from request
// 2. Include Server header
//

// Actual Payload Format
// RTSP/1.0 200 OK\r\n
// CSeq: <from request>
// Content-Type: <based on content>
// Header1: Value1\r\n
// Header2: Value2\r\n
// <more headers each followed by \r\n>
// Content-Length: <val>  <<-- if there is content
// \r\n  <<-- separate headers from content
// <binary or plist content>
//
// >> write data to socket <<

class Reply; // forward decl for shared_ptr typedef

typedef std::shared_ptr<Reply> ReplyShared;

class Reply : public Headers {
public:
  enum RespCode : uint16_t {
    OK = 200,
    BadRequest = 400,
    Unauthorized = 403,
    Unavailable = 451,
    NotImplemented = 501
  };

public:
  typedef vector<char8_t> Payload;
  typedef fmt::basic_memory_buffer<char, 128> Packet;
  typedef tuple<const char *, size_t> PacketInfo;

  typedef const unordered_map<RespCode, const char *> RespCodeMap;

public:
  static ReplyShared create(RequestShared request);

public:
  Reply(RequestShared request);

  const Packet &build();
  void dump() const;
  virtual bool populate() = 0;
  inline void responseCode(RespCode code) { _rcode = code; }

protected:
  string_view _method;
  string_view _path;

  RespCode _rcode = RespCode::NotImplemented;

  RequestShared _request;
  Payload _payload;
  Packet _packet;

private:
  bool assemblePacket();
  void init(RequestShared request);

  //
};

} // namespace rtsp
} // namespace pierre