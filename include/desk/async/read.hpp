
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

#include "async/matcher.hpp"
#include "base/types.hpp"

#include <boost/asio/read_until.hpp>
#include <memory>
#include <type_traits>

namespace pierre {
namespace desk {
namespace async {

/// @brief Read a Desk Msg
/// @param sock Socket to read from
/// @param msg Msg for storing data and eventually returned to the caller
/// @param token Completion Token
/// @return asio::async_result
template <typename Socket, typename M, typename CompletionToken>
auto read_msg(Socket &sock, M &&msg, CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, Socket &sock, M msg) {
    struct intermediate_completion_handler {
      Socket &sock;
      M msg; // hold the in-flight message
      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, size_t n = 0) noexcept {
        // convert the async operation to our preferred handler
        msg(ec, n);

        handler(std::move(msg));
      }

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                      typename Socket::executor_type>;

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

    msg.reuse();
    auto &buffer = msg.buffer(); // get the buffer, it may have pending data

    // ok, we have everything we need kick-off async_read_until()
    asio::async_read_until( //
        sock, buffer,       //
        async::matcher(),   //
        intermediate_completion_handler{
            // reference to socket
            sock,
            // the message (logic)
            std::move(msg),
            // handler
            std::forward<decltype(completion_handler)>(completion_handler)});
  };

  return asio::async_initiate<CompletionToken, void(M && msg)>(
      initiation,          // initiation function object
      token,               // user supplied callback
      std::ref(sock),      // socket and data storage
      std::forward<M>(msg) // the message
  );
}

} // namespace async
} // namespace desk
} // namespace pierre