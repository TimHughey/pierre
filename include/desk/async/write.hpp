
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

/// @brief Async write desk msg
/// @tparam M
/// @tparam CompletionToken
/// @param socket
/// @param msg
/// @param token
/// @return
template <typename M, typename CompletionToken>
auto write_msg(tcp_socket &sock, M &&msg, CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, tcp_socket &sock, M msg) {
    struct intermediate_completion_handler {
      tcp_socket &sock;
      M msg;
      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, size_t n) noexcept {
        msg.ec = ec;
        msg.xfr.out = n;

        // INFO("desk.async", "write_msg", "wrote {} bytes\n", n);

        handler(std::move(msg)); // call user-supplied handler
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
    };

    // must grab the buff_seq and tx_len BEFORE moving the msg
    auto &buffer = msg.buffer();

    // initiate the actual async operation
    asio::async_write(
        sock, buffer,
        intermediate_completion_handler{
            sock, std::move(msg), std::forward<decltype(completion_handler)>(completion_handler)});
  };

  msg.serialize();

  // initiate the async operation
  return asio::async_initiate<CompletionToken, void(M msg)>(
      initiation,          // initiation function object
      token,               // user supplied callback
      std::ref(sock),      //
      std::forward<M>(msg) // move the msg for use within the async operation
  );
}

} // namespace async
} // namespace desk
} // namespace pierre