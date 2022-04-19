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
#include "rtsp/reply/method.hpp"
#include "rtsp/reply/packet_out.hpp"

namespace pierre {
namespace rtsp {
using namespace reply;

using std::string, std::string_view, std::unordered_map, std::back_inserter;
using enum RespCode;

typedef const std::string &csr;

static RespCodeMap _resp_text{{OK, "OK"},
                              {AuthRequired, "Connection Authorization Required"},
                              {BadRequest, "Bad Request"},
                              {InternalServerError, "Internal Server Error"},
                              {Unauthorized, "Unauthorized"},
                              {Unavailable, "Unavailable"},
                              {NotImplemented, "Not Implemented"}};

// this static member function is in the .cpp due to the call to Factory to
// create the approprite Reply subclass
[[nodiscard]] sReply Reply::create(const Reply::Opts &opts) { return Factory::create(opts); }

Reply::Reply(const Reply::Opts &opts)
    : Method(opts.method),            // request method
      Path(opts.path),                // request path
      _host(opts.host),               // local shared_ptr to Host
      _service(opts.service),         // local shared_ptr to Service
      _aes_ctx(opts.aes_ctx),         // local shared_ptr to AexCtx
      _mdns(opts.mdns),               // local shared_ptr to mDNS
      _nptp(opts.nptp),               // local shared_ptr to Nptp
      _rtp(opts.rtp),                 // local shared_ptr to Rtp
      _request_content(opts.content), // request content
      _request_headers(opts.headers)  // reqquest headers

{
  // copy the sequence header, must be part of the reply headers
  headerCopy(opts.headers, Headers::Type2::CSeq);
  headerAdd(Server, AirPierre);
}

[[nodiscard]] PacketOut &Reply::build() {
  constexpr auto seperator = "\r\n";
  [[maybe_unused]] const auto ok = populate();

  auto where = back_inserter(_packet);
  auto resp_text = _resp_text.at(_rcode);

  // fmt::print("Reply::build() --> {} {:#} {}\n", ok, _rcode, resp_text);

  fmt::format_to(where, "RTSP/1.0 {:d} {}{}", _rcode, resp_text, seperator);

  if (_content.empty() == false) {
    headerAdd(ContentLength, _content.size());
  }

  auto headers_list = headersList();

  std::copy(headers_list.begin(), headers_list.end(), where);

  // always write the separator between heqders and content
  fmt::format_to(where, "{}", seperator);

  if (_content.empty() == false) {
    // we have content for the reply, add it now
    std::copy(_content.begin(), _content.end(), where);
  }

  // fmt::print("Reply::build(): final packet size={}", _packet.size());

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
  auto end = begin + bytes;

  // fmt::print("copying from={},{} to={}\n", fmt::ptr(begin), fmt::ptr(end),
  //           fmt::ptr(&(*where)));

  std::copy(begin, end, where);
}

void Reply::dump() const { headersDump(); }

} // namespace rtsp
} // namespace pierre