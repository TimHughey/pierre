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
#include "common/typedefs.hpp"
#include "packet/headers.hpp"
#include "packet/out.hpp"
#include "packet/resp_code.hpp"
#include "reply/factory.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <iterator>
#include <typeinfo>

namespace pierre {
namespace airplay {
namespace reply {

using enum packet::RespCode;
namespace header = pierre::packet::header;

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

[[nodiscard]] packet::Out &Reply::build() {
  constexpr auto seperator = "\r\n";

  [[maybe_unused]] const auto ok = populate();

  auto where = back_inserter(_packet);
  auto resp_text = respCodeToView(_rcode);

  fmt::format_to(where, "RTSP/1.0 {:d} {}{}", _rcode, resp_text, seperator);

  if (!_content.empty()) { // add the content length before adding the header list
    headers.add(header::type::ContentLength, _content.size());
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

    constexpr auto f = FMT_STRING("{} REPLY FINAL cseq={:<04} fb={:<04} "
                                  "size={:<05} rc={:<15} method={:<19} "
                                  "path={}\n");

    if (di->path == feedback) {
      ++feedbacks;
    }

    if (di->path != csv("/feedback")) {
      fmt::print(f, runTicks(), headers.getValInt(header::type::CSeq), feedbacks, _packet.size(),
                 resp_text, di->method, di->path);
    }
  }

  return _packet;
}

void Reply::copyToContent(const fmt::memory_buffer &buffer) {
  const auto begin = buffer.begin();
  const auto end = buffer.end();

  auto where = std::back_inserter(_content);

  std::copy(begin, end, where);
}

void Reply::copyToContent(std::shared_ptr<uint8_t[]> data, const size_t bytes) {
  // const uint8_t *begin = data.get();

  copyToContent(data.get(), bytes);
}

void Reply::copyToContent(const uint8_t *begin, const size_t bytes) {
  auto where = std::back_inserter(_content);
  std::copy(begin, begin + bytes, where);
}

Reply &Reply::inject(const reply::Inject &injected) {
  // copy the sequence header, must be part of the reply headers
  headers.copy(injected.headers, header::type::CSeq);
  headers.add(header::type::Server, header::val::AirPierre);

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