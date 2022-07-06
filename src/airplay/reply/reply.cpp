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
#include "reply/factory.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <iterator>
#include <typeinfo>

namespace pierre {
namespace airplay {
namespace reply {

// this static member function is in the .cpp due to the call to Factory to
// create the approprite Reply subclass
[[nodiscard]] shReply Reply::create(const reply::Inject &di) {
  if (false) { // false
    if (di.path != csv("/feedback")) {
      di.headers.dump();
    }
  }
  auto reply = Factory::create(di);

  // inject dependencies after initial creation
  reply->inject(di);

  return reply;
}

[[nodiscard]] uint8v &Reply::build() {
  constexpr csv seperator("\r\n");

  [[maybe_unused]] const auto ok = populate();

  _packet.clear();

  auto where = back_inserter(_packet);
  auto resp_text = respCodeToView(_rcode);

  fmt::format_to(where, "RTSP/1.0 {:d} {}{}", _rcode, resp_text, seperator);

  if (!_content.empty()) { // add the content length before adding the header list
    headers.add(hdr_type::ContentLength, _content.size());
  }

  headers.list(where);

  // always write the separator between heqders and content
  fmt::format_to(where, "{}", seperator);

  if (_content.empty() == false) {
    std::copy(_content.begin(), _content.end(), where); // we have content, add it now
  }

  if (true) { // debug
    constexpr auto feedback = csv("/feedback");
    static uint64_t feedbacks = 0;

    if (di->path == feedback) {
      ++feedbacks;
    }

    if (di->path != feedback) {
      __LOG0("{:<18} cseq={:>4} fb={:>4} size={:>4} rc={:<15} method={:<19} path={}\n", //
             moduleID(), headers.getValInt(hdr_type::CSeq), feedbacks, _packet.size(), resp_text,
             di->method, di->path);
    }
  }

  return _packet;
}

void Reply::copyToContent(std::shared_ptr<uint8_t[]> data, const size_t bytes) {
  copyToContent(data.get(), bytes);
}

void Reply::copyToContent(const uint8_t *begin, const size_t bytes) {
  auto where = std::back_inserter(_content);
  std::copy(begin, begin + bytes, where);
}

Reply &Reply::inject(const reply::Inject &injected) {
  // copy the sequence header, must be part of the reply headers
  headers.copy(injected.headers, hdr_type::CSeq);
  headers.add(hdr_type::Server, hdr_val::AirPierre);

  di.emplace(injected); // use an optional because it contains references

  return *this;
}

// misc debug
bool Reply::debugFlag(bool debug_flag) {
  _debug_flag = debug_flag;
  return _debug_flag;
}

void Reply::dump() const { headers.dump(); }

} // namespace reply
} // namespace airplay
} // namespace pierre