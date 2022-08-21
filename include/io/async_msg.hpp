
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

namespace pierre {

struct async_read_msg_impl {
  tcp_socket &socket;
  uint8v &buff;
  enum { starting, msg_content, finished } state = starting;

  template <typename Self> void operator()(Self &self, error_code ec = {}, size_t n = 0) {

    switch (state) {
    case starting:
      state = msg_content;
      socket.async_read_some(asio::buffer(buff.data(), sizeof(uint16_t)),
                             asio::bind_executor(socket.get_executor(), std::move(self)));
      break;

    case msg_content:
      if (!ec) {
        state = finished;
        socket.async_read_some(asio::buffer(buff.data(), msg_len()),
                               asio::bind_executor(socket.get_executor(), std::move(self)));
      } else {
        self.complete(ec, n);
      }
      break;

    case finished:
      self.complete(ec, n);
      break;
    }
  }

private:
  uint16_t msg_len() {
    uint16_t *msg_len_ptr = (uint16_t *)buff.data();
    return ntohs(*msg_len_ptr);
  }

  void log(const error_code &ec, size_t n) {
    __LOG0(LCOL01 " state={} len={} reason={}\n", Desk::moduleID(), "IMPL", state, n,
           ec.message());
  }
};

template <typename CompletionToken>
auto async_read_msg(tcp_socket &socket, uint8v &buff, CompletionToken &&token) ->
    typename asio::async_result<typename std::decay<CompletionToken>::type,
                                void(error_code, size_t)>::return_type {
  return asio::async_compose<CompletionToken, void(error_code, size_t)>(
      async_read_msg_impl(socket, buff), token, socket);
}

} // namespace pierre