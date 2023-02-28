
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
#include "desk/msg.hpp"
#include "io/io.hpp"

#include <array>
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
template <typename CompletionToken> auto read_msg(tcp_socket &socket, CompletionToken &&token) {
  auto initiation = [](auto &&completion_handler, tcp_socket &socket, Msg msg) {
    struct intermediate_completion_handler {
      tcp_socket &socket; // for the second async_read() and obtaining the I/O executor
      Msg msg;            // hold the in-flight Msg
      enum { msg_header, msg_content, deserialize } state;
      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, std::size_t n = 0) noexcept {

        msg.xfr.in += n;
        msg.ec = ec;

        if (!msg.ec) {

          switch (state) {
          case msg_header:
          case msg_content: {
            state = deserialize;

            auto buff = msg.buff_packed();
            asio::async_read(socket, buff, std::move(*this));

            return; // async operation not complete yet
          }

          case deserialize:
            if (msg.deserialize(n) == false) {
              msg.ec = io::make_error(errc::protocol_error);
            }
          }
        }

        // This point is reached only on completion of the entire composed
        // operation.

        // Call the user-supplied handler with the result of the operation.
        handler(std::move(msg));
      };

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
    }; // end intermediate struct

    // Initiate the underlying async_read operation using our intermediate
    // completion handler.

    auto buff = msg.buff_msg_len();
    asio::async_read(socket, buff,
                     intermediate_completion_handler{
                         socket,                                      // the socket
                         std::move(msg),                              // the msg
                         intermediate_completion_handler::msg_header, // initial state
                         std::forward<decltype(completion_handler)>(completion_handler) // handler
                     });
  };

  return asio::async_initiate<CompletionToken, void(Msg msg)>(
      initiation,       // initiation function object
      token,            // user supplied callback
      std::ref(socket), // wrap non-const args to prevent incorrect decay-copies
      Msg()             // create for use within the async operation
  );
}

/// @brief Async write desk msg
/// @tparam M
/// @tparam CompletionToken
/// @param socket
/// @param msg
/// @param token
/// @return
template <typename M, typename CompletionToken>
auto write_msg(tcp_socket &socket, M &&msg, CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, tcp_socket &socket, M &&msg) {
    struct intermediate_completion_handler {
      tcp_socket &socket; // for multiple write ops and obtaining I/O executor
      M msg;

      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, std::size_t n) noexcept {
        msg.ec = ec;
        msg.xfr.out = n;

        handler(std::move(msg)); // call user-supplied handler
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

    // initiate the actual async operation
    asio::async_write(socket, buff_seq,
                      intermediate_completion_handler{
                          socket, std::forward<M>(msg),
                          std::forward<decltype(completion_handler)>(completion_handler)});
  };

  msg.finalize();
  msg.serialize();

  // initiate the async operation
  return asio::async_initiate<CompletionToken, void(M msg)>(
      initiation,          // initiation function object
      token,               // user supplied callback
      std::ref(socket),    // wrap non-const args to prevent incorrect decay-copies
      std::forward<M>(msg) // move the msg for use within the async operation
  );
}

} // namespace async
} // namespace desk
} // namespace pierre