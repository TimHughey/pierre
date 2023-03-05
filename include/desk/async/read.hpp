
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

#include "base/types.hpp"
#include "io/error.hpp"
#include "io/read_write.hpp"
#include "io/tcp.hpp"
#include "lcs/logger.hpp"

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace pierre {
namespace desk {
namespace async {

/// @brief Async read desk msg
/// @tparam CompletionToken
/// @param socket
/// @param token
/// @return
template <typename M, typename Storage, typename CompletionToken>
auto read_msg(tcp_socket &sock, Storage &storage, M &&msg, CompletionToken &&token) {
  auto initiation = [](auto &&completion_handler, tcp_socket &sock, Storage &storage, M msg) {
    struct intermediate_completion_handler {
      tcp_socket &sock;
      Storage &storage;
      M msg; // hold the in-flight M
      enum { msg_header, msg_content } state;
      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, size_t n = 0) noexcept {

        msg.xfr.in += n;
        msg.ec = ec;

        if (!msg.ec) {

          switch (state) {
          case msg_header:
            if (msg.calc_packed_len(storage, n) == false) {
              state = msg_content;
              auto read_bytes = msg.read_bytes();

              asio::async_read(sock, storage, std::move(read_bytes), std::move(*this));
              return;
            }

          case msg_content:
            break;
          }
        }

        // this point is reached only on completion of the entire composed operation.
        // call the user-supplied handler with the result of the operation.
        handler(std::move(msg));
      }

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                      tcp_socket::executor_type>;

      executor_type get_executor() const noexcept {
        return asio::get_associated_executor(handler, sock.get_executor());
      }

      using allocator_type =
          asio::associated_allocator_t<typename std::decay<decltype(completion_handler)>::type,
                                       std::allocator<void>>;

      allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(handler, std::allocator<void>{});
      }
    }; // end intermediate struct

    // Initiate the underlying async_read operation using our intermediate
    // completion handler.

    asio::async_read(sock, storage, asio::transfer_at_least(msg.hdr_bytes),
                     intermediate_completion_handler{
                         // references to socket and storage
                         sock, storage,
                         // the message (logic)
                         std::move(msg),
                         // initial state
                         intermediate_completion_handler::msg_header,
                         // handler
                         std::forward<decltype(completion_handler)>(completion_handler)});
  };

  // check if there is already a message waiting in the stream buffer
  if (msg.calc_packed_len(storage)) {
    // yes, replicate asio behavior and post to the handler

    asio::post(sock.get_executor(), [msg = std::move(msg), token = std::move(token)]() mutable {
      typename std::decay<decltype(token)>::type handler = std::move(token);
      handler(std::move(msg));
    });

  } else {

    return asio::async_initiate<CompletionToken, void(M msg)>(
        initiation,                        // initiation function object
        token,                             // user supplied callback
        std::ref(sock), std::ref(storage), // socket and data storage
        std::forward<M>(msg)               // user supplied message
    );
  }
}

} // namespace async
} // namespace desk
} // namespace pierre