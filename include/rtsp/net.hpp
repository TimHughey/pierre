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
#include "base/uint8v.hpp"
#include "io/io.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "rtsp/aes.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"
#include "rtsp/saver.hpp"

namespace pierre {

namespace rtsp {

template <typename CTX, typename CompletionToken>
auto async_read_msg(const CTX ctx, CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, const CTX ctx, Request &request) {
    struct intermediate_completion_handler {
      const CTX ctx;
      Request &request;
      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, [[maybe_unused]] std::size_t bytes = 0) {

        INFO("rtsp.net", "read", "bytes={}\n", bytes);

        if (ec) {
          handler(ec); // end of composed operation
          return;
        }

        const auto sock = ctx->sock;
        auto &aes = ctx->aes;
        auto buff = request.buffer();

        // if there is more data waiting on the socket read it
        if (size_t avail = sock->available(); avail > 0) {
          INFO("rtsp.net", "available", "bytes={}\n", avail);
          // special case: we know bytes are available, do sync read

          error_code ec;
          size_t bytes = asio::read(*sock, buff, asio::transfer_exactly(avail), ec);

          if ((ec) || (bytes != avail)) {
            handler(ec ? ec : io::make_error(errc::message_size));
            return;
          }
        }

        if (const auto buffered = std::ssize(request.wire); buffered > 0) {
          INFO("rtsp.net", "buffered", "buffered={}\n", request.wire.size());
          if (auto consumed = aes.decrypt(request.wire, request.packet); consumed != buffered) {

            // incomplete decipher, read from socket
            asio::async_read(*sock, buff, asio::transfer_at_least(1), std::move(*this));
            return;
          }
        }

        INFO("rtsp.net", "consumed", "wire={} packet={}\n", request.wire.size(),
             request.packet.size());

        // we potentially have a complete message, attempt to find the delimiters
        if (request.find_delims() == false) {

          // we need more data in the packet to continue
          // incomplete decipher, read from socket
          asio::async_read(*sock, buff, asio::transfer_at_least(1), std::move(*this));
          return;
        }

        INFO("rtsp.net", "find_delms", "delims={}\n", request.delims.size());

        if (request.headers.parse_ok == false) {
          // we haven't parsed headers yet, do it now
          auto parse_ok = request.headers.parse(request.packet, request.delims);

          if (parse_ok == false) {
            handler(io::make_error(errc::bad_message));
            return;
          }
        }

        if (const auto more_bytes = request.populate_content(); more_bytes > 0) {
          asio::async_read(*sock, buff, asio::transfer_exactly(more_bytes), std::move(*this));
          return;
        }

        if (!ec) {
          Saver(Saver::IN, request.headers, request.content);
        }

        handler(ec); // end of composed operation

      } // end operator()

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                      tcp_socket::executor_type>;

      executor_type get_executor() const noexcept {
        return asio::get_associated_executor(handler, ctx->sock->get_executor());
      }

      using allocator_type =
          asio::associated_allocator_t<typename std::decay<decltype(completion_handler)>::type,
                                       std::allocator<void>>;

      allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(handler, std::allocator<void>{});
      }
    }; // intermediate_completion_handler struct end

    // initiate the underlying async_read using our intermediate completion handler
    asio::async_read(*(ctx->sock), request.buffer(), asio::transfer_at_least(117),
                     intermediate_completion_handler{
                         ctx,
                         request, // the request
                         std::forward<decltype(completion_handler)>(completion_handler) // handler
                     });
  }; // initiation lambda end

  return asio::async_initiate<CompletionToken, void(error_code)>(
      initiation,               // initiation function object
      token,                    // user supplied callback
      ctx,                      // the overall context
      std::ref(*(ctx->request)) // non-const reference to request
  );
}

template <typename CTX, typename CompletionToken>
auto async_write_msg(const CTX ctx, CompletionToken &&token) {

  auto initiation = [](auto &&completion_handler, const CTX ctx, Reply &reply) {
    struct intermediate_completion_handler {
      const CTX ctx;
      Reply &reply;
      typename std::decay<decltype(completion_handler)>::type handler;

      void operator()(const error_code &ec, std::size_t bytes) {

        INFO("rtsp.net", "write", "bytes={}\n", bytes);

        Stats::write(stats::RTSP_SESSION_TX_REPLY, static_cast<int64_t>(bytes));

        if (!ec) {
          Saver(Saver::OUT, reply.headers_out, reply.content_out, reply.resp_code);

          handler(ec); // call user-supplied handler}
          return;
        }
      } // end operator

      using executor_type =
          asio::associated_executor_t<typename std::decay<decltype(completion_handler)>::type,
                                      tcp_socket::executor_type>;

      executor_type get_executor() const noexcept {
        return asio::get_associated_executor(handler, ctx->sock->get_executor());
      }

      using allocator_type =
          asio::associated_allocator_t<typename std::decay<decltype(completion_handler)>::type,
                                       std::allocator<void>>;

      allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(handler, std::allocator<void>{});
      }
    };

    const auto sock = ctx->sock;
    ctx->aes.encrypt(reply.wire); // NOTE: noop until cipher exchange completed

    // initiate the actual async operation
    asio::async_write(*sock, reply.buffer(),
                      intermediate_completion_handler{ctx, reply,
                                                      std::forward<decltype(completion_handler)>(
                                                          completion_handler)}); // forward token
  };

  // initiate the async operation
  return asio::async_initiate<CompletionToken, void(error_code)>(
      initiation, // initiation function object
      token,      // user supplied callback
      ctx,
      std::ref(*(ctx->reply))); // move the msg for use within the async operation
}

} // namespace rtsp
} // namespace pierre