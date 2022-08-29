
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

#include "io/io.hpp"
#include "io/msg.hpp"

#include <array>
#include <memory>
#include <type_traits>

namespace pierre {
namespace io {

class async_tld_impl {

public:
  async_tld_impl(tcp_socket &socket, Packed &buff, StaticDoc &doc) //
      : socket(socket), buff(buff), doc(doc) {
    doc.clear(); // doc maybe reused, clear it
  }

  template <typename Self> void operator()(Self &self, error_code ec = {}, size_t n = 0) {

    switch (state) {
    case starting:
      state = msg_content;
      socket.async_read_some(asio::buffer(buff.data(), MSG_LEN_BYTES),
                             asio::bind_executor(socket.get_executor(), std::move(self)));
      break;

    case msg_content:
      state = finish;
      if (!ec) {
        auto *p = reinterpret_cast<uint16_t *>(buff.data());
        packed_len = ntohs(*p);
        socket.async_read_some(asio::buffer(buff.data(), packed_len),
                               asio::bind_executor(socket.get_executor(), std::move(self)));
      } else {
        self.complete(ec, n);
      }
      break;

    case finish:
      if (!ec) {
        if (auto err = deserializeMsgPack(doc, buff.data(), packed_len); err) {
          self.complete(make_error(errc::protocol_error), 0);
        } else {
          self.complete(make_error(errc::success), doc.size());
        }
      } else {
        self.complete(ec, n);
      }
      break;
    }
  }

private:
  // order dependent
  tcp_socket &socket;
  enum { starting, msg_content, finish } state = starting;
  Packed &buff;
  StaticDoc &doc;
  uint16_t packed_len = 0; // calced in msg_content, used in finish

  // const
  static constexpr auto MSG_LEN_BYTES = sizeof(uint16_t);
};

template <typename CompletionToken>
auto async_tld(tcp_socket &socket, Packed &work_buff, StaticDoc &doc, CompletionToken &&token) ->
    typename asio::async_result<typename std::decay<CompletionToken>::type,
                                void(error_code, size_t)>::return_type {
  return asio::async_compose<CompletionToken, void(error_code, size_t)>(
      async_tld_impl(socket, work_buff, doc), token, socket);
}

} // namespace io
} // namespace pierre