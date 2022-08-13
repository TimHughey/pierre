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

#include "reply/reply.hpp"
#include "base/headers.hpp"
#include "base/resp_code.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <typeinfo>

namespace pierre {
namespace airplay {
namespace reply {

[[nodiscard]] uint8v &Reply::build() {
  constexpr csv seperator("\r\n");

  [[maybe_unused]] const auto ok = populate();

  _packet.clear();

  auto where = back_inserter(_packet);
  auto resp_text = respCodeToView(_rcode);

  fmt::format_to(where, "RTSP/1.0 {:d} {}{}", _rcode, resp_text, seperator);

  // must add content length before calling headers list()
  if (!_content.empty()) {
    headers.add(hdr_type::ContentLength, _content.size());
  }

  headers.list(where);

  // always write the separator between heqders and content
  fmt::format_to(where, "{}", seperator);

  if (_content.empty() == false) { // we have content, add it
    std::copy(_content.begin(), _content.end(), where);
  }

  log_reply(resp_text);

  return _packet;
}

Reply &Reply::inject(const reply::Inject &injected) {
  // copy the sequence header, must be part of the reply headers
  headers.copy(injected.headers, hdr_type::CSeq);
  headers.add(hdr_type::Server, hdr_val::AirPierre);

  di.emplace(injected); // use an optional because it contains references

  return *this;
}

// misc debug
void Reply::dump() const { headers.dump(); }

void Reply::log_reply(csv resp_text) {
  constexpr std::array no_log{csv("GET"),       csv("GET_PARAMETER"), csv("RECORD"),
                              csv("SETPEERSX"), csv("SET_PARAMETER"), csv("POST"),
                              csv("SETUP"),     csv("FEEDBACK"),      csv("SETRATEANCHORTIME"),
                              csv("TEARDOWN")};

  if (di.has_value()) {
    if (ranges::none_of(no_log, [&](csv m) { return di->method == m; })) {
      __LOG0(LCOL01 " cseq={:>4} size={:>4} rc={:<15} method={:<19} path={}\n", //
             moduleID(), "REPLY", headers.getValInt(hdr_type::CSeq), _packet.size(), resp_text,
             di->method, di->path);
    }
  }
}

} // namespace reply
} // namespace airplay
} // namespace pierre