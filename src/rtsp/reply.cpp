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

#include <algorithm>
#include <fmt/format.h>
#include <iterator>
#include <string_view>
#include <typeinfo>
#include <unordered_map>

#include "rtsp/headers.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/reply/factory.hpp"
#include "rtsp/reply/packet_out.hpp"

namespace pierre {
namespace rtsp {

using std::string, std::string_view, std::unordered_map, std::back_inserter;
using enum RespCode;

static RespCodeMap _resp_text{{OK, "OK"},
                              {AuthRequired, "Connection Authorization Required"},
                              {BadRequest, "Bad Request"},
                              {InternalServerError, "Internal Server Error"},
                              {Unauthorized, "Unauthorized"},
                              {Unavailable, "Unavailable"},
                              {NotImplemented, "Not Implemented"}};

Reply::Reply(sRequest request) {
  // all constructors call init() to complete initialization
  init(request);
}

[[nodiscard]] sReply Reply::create(sRequest request) { return Factory::create(request); }

[[nodiscard]] PacketOut &Reply::build() {
  const auto ok = populate();

  auto where = back_inserter(_packet);
  auto resp_text = _resp_text.at(_rcode);

  if (_content.empty() == false) {
    headerAdd(ContentLength, _content.size());
  }

  fmt::print("Reply::build() --> {} {:#} {}\n", ok, _rcode, resp_text);

  fmt::format_to(where, "RTSP/1.0 {:d} {}\r\n", _rcode, resp_text);

  auto headers_list = headersList();

  std::copy(headers_list.begin(), headers_list.end(), where);

  if (_content.empty() == false) {
    fmt::format_to(where, "\r\n");
    std::copy(_content.begin(), _content.end(), where);
  }

  // prevent circular shared ptr locks
  _request.reset();

  return _packet;
}

void Reply::copyToContent(const uint8_t *begin, const size_t bytes) {
  auto where = std::back_inserter(_content);
  auto end = begin + bytes;

  // fmt::print("copying from={},{} to={}\n", fmt::ptr(begin), fmt::ptr(end),
  //           fmt::ptr(&(*where)));

  std::copy(begin, end, where);
}

void Reply::dump() const { headersDump(); }

void Reply::init(sRequest request) {
  //
  _request = request;

  headerCopy(*request, Headers::Type2::CSeq);
  headerAdd(Server, AirPierre);
}

} // namespace rtsp
} // namespace pierre