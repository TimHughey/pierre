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

namespace pierre {
namespace rtsp {

using std::string, std::string_view, std::unordered_map, std::back_inserter;
using enum Reply::RespCode;

static Reply::RespCodeMap _resp_text{{OK, "OK"},
                                     {BadRequest, "Bad Request"},
                                     {Unauthorized, "Unauthorized"},
                                     {Unavailable, "Unavailable"},
                                     {NotImplemented, "Not Implemented"}};

Reply::Reply(RequestShared request) {
  // all constructors call init() to complete initialization
  init(request);
}

[[nodiscard]] ReplyShared Reply::create(RequestShared request) { return Factory::create(request); }

[[nodiscard]] const Reply::Packet &Reply::build() {
  const auto ok = populate();

  if (!ok) {
    _packet.clear();

    return _packet;
  }

  auto where = back_inserter(_packet);
  auto resp_text = _resp_text.at(_rcode);

  fmt::format_to(where, "RTSP/1.0 {:d} {}\r\n", _rcode, resp_text);

  auto headers_list = headersList();
  const auto headers_begin = headers_list.data();
  const auto headers_end = headers_begin + headers_list.size();

  _packet.append(headers_begin, headers_end);

  where = back_inserter(_packet);
  fmt::format_to(where, "\r\n");

  const auto content_begin = _payload.data();
  const auto content_end = content_begin + _payload.size();
  _packet.append(content_begin, content_end);

  // prevent circular shared ptr lpcks
  _request.reset();

  return _packet;
}

void Reply::dump() const { headersDump(); }

void Reply::init(RequestShared request) {
  //
  _request = request;

  headerCopy(*request, Headers::Type2::CSeq);
  headerAdd(Server, AirPierre);
}

} // namespace rtsp
} // namespace pierre