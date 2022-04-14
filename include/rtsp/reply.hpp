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

#include "rtsp/content.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/reply/packet_out.hpp"
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

typedef std::tuple<char *, size_t> PacketInInfo;
typedef std::shared_ptr<Reply> sReply;
typedef const unordered_map<RespCode, const char *> RespCodeMap;

class Reply : public Headers {
public:
  static sReply create(sRequest request);

public:
  Reply(sRequest request);

  PacketOut &build();
  void copyToContent(const uint8_t *begin, const size_t bytes);
  auto &errMsg() { return _err_msg; }
  void dump() const;
  virtual bool populate() = 0;
  inline void responseCode(RespCode code) { _rcode = code; }

protected:
  string_view _method;
  string_view _path;
  string _err_msg;

  RespCode _rcode = RespCode::NotImplemented;

  sRequest _request;
  Content _content;
  PacketOut _packet;

private:
  void init(sRequest request);

  //
};

} // namespace rtsp
} // namespace pierre