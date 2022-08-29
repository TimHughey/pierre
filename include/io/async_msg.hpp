
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

#include <array>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

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

// async_write_msg(): write a message object to the socket
template <typename M, typename CompletionToken>
auto async_write_msg(tcp_socket &socket, M msg, CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, tcp_socket &socket, M msg) {
    struct intermediate_completion_handler {
      tcp_socket &socket; // for multiple write ops and obtaining I/O executor
      M msg;

      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, std::size_t bytes) {
        msg.log_tx(ec, bytes);

        handler(ec); // call user-supplied handler
      }

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                      tcp_socket::executor_type>;

      executor_type get_executor() const noexcept {
        return asio::get_associated_executor(handler, socket.get_executor());
      }

      using allocator_type =
          asio::associated_allocator_t<typename std::decay<decltype(completion_handler)>::type,
                                       std::allocator<void>>;

      allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(handler, std::allocator<void>{});
      }
    };

    // must grab the buff_seq and tx_len BEFORE moving the msg
    auto buff_seq = msg.buff_seq();
    auto tx_len = msg.tx_len;

    // initiate the actual async operation
    asio::async_write(
        socket, buff_seq, asio::transfer_exactly(tx_len),
        intermediate_completion_handler{
            socket,                                                        // pass the socket along
            std::move(msg),                                                // move the msg along
            std::forward<decltype(completion_handler)>(completion_handler) // forward token
        });
  };

  // initiate the async operation
  return asio::async_initiate<CompletionToken, void(error_code)>(
      initiation,       // initiation function object
      token,            // user supplied callback
      std::ref(socket), // wrap non-const args to prevent incorrect decay-copies
      std::move(msg)    // move the msg for use within the async operation
  );
}

} // namespace io
} // namespace pierre