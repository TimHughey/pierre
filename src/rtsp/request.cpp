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

#include "request.hpp"

#include <exception>
#include <span>
#include <utility>

namespace pierre {
namespace rtsp {

void Request::parse_headers() {
  if (headers.parse_ok == false) {
    // we haven't parsed headers yet, do it now
    auto parse_ok = headers.parse(packet, delims);

    if (parse_ok == false) throw(std::runtime_error(headers.parse_err));
  }
}

size_t Request::populate_content() noexcept {

  long more_bytes{0}; // assume we have all content

  if (headers.contains(hdr_type::ContentLength)) {
    const auto expected_len = headers.val<int64_t>(hdr_type::ContentLength);

    // validate we have a packet that contains all the content
    const auto content_begin = delims[1].first + delims[1].second;
    const auto content_avail = std::ssize(packet) - content_begin;

    // compare the content_end to the size of the packet
    if (content_avail == expected_len) {
      // packet contains all the content, create a span representing the content in the packet
      content.assign_span(std::span(packet.from_begin(content_begin), expected_len));
    } else {
      // packet does not contain all the content, read bytes from the socket
      more_bytes = expected_len - content_avail;
    }
  }

  return more_bytes;
}

} // namespace rtsp
} // namespace pierre