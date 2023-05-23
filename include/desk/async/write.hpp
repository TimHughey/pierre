
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

#include <boost/asio/write.hpp>
#include <memory>
#include <type_traits>

namespace pierre {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

namespace desk {
namespace async {

/// @brief Write a Desk Msg
/// @param sock Socket to write to
/// @param msg Msg containing payload and eventually returned to the caller
/// @param token Completion Token
/// @return asio::async_result
template <typename Socket, typename M, typename CompletionToken>
auto write_msg(Socket &socket, M &&msg, CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, Socket &socket, M &&msg) {
    struct intermediate_completion_handler {
      Socket &socket; // for multiple write ops and obtaining I/O executor
      M msg;
      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, std::size_t n) noexcept {
        msg(ec, n);

        handler(std::move(msg)); // call user-supplied handler
      }

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                      typename Socket::executor_type>;

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

    msg.serialize();
    auto &buffer = msg.buffer();

    // initiate the actual async operation
    asio::async_write(socket, buffer,
                      intermediate_completion_handler{
                          socket, std::move(msg),
                          std::forward<decltype(completion_handler)>(completion_handler)});
  };

  // initiate the async operation
  return asio::async_initiate<CompletionToken, void(M msg)>(
      initiation,       // initiation function object
      token,            // user supplied callback
      std::ref(socket), // wrap non-const args to prevent incorrect decay-copies
      std::move(msg)    // move the msg for use within the async operation
  );
}

} // namespace async
} // namespace desk
} // namespace pierre