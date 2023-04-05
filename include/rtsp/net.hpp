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
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "rtsp/aes.hpp"
#include "rtsp/headers.hpp"
#include "rtsp/saver.hpp"

#include <boost/asio/read.hpp>
#include <boost/system/errc.hpp>
#include <boost/system/error_code.hpp>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;

namespace rtsp {

struct net {

  /// @brief Read a full message async
  /// @tparam R container for the read message
  /// @tparam CompletionToken
  /// @param sock
  /// @param r
  /// @param aes
  /// @param token
  /// @return
  template <typename R, typename CompletionToken>
  static auto async_read_msg(tcp_socket &sock, R &r, Aes &aes, CompletionToken &&token) {

    auto initiation = [](auto &&completion_handler, tcp_socket &sock, R &r, Aes &aes) {
      struct intermediate_completion_handler {
        tcp_socket &sock;
        R &r;
        Aes &aes;
        typename std::decay<decltype(completion_handler)>::type handler;

        void operator()(const error_code &ec, std::size_t bytes = 0) {
          INFO("rtsp.net", "read", "bytes={}\n", bytes);

          if (r.wire.empty() && r.packet.empty()) r.e.reset();

          if (ec) {
            handler(ec); // end of composed operation
            return;
          }

          // if there is more data waiting on the socket read it
          if (size_t avail = sock.available(); avail > 0) {
            INFO("rtsp.net", "available", "bytes={}\n", avail);
            // special case: we know bytes are available, do sync read
            error_code ec;
            size_t bytes = asio::read(sock, r.buffer(), asio::transfer_exactly(avail), ec);

            if (ec || (bytes != avail)) {
              handler(ec ? ec : error_code(errc::message_size, sys::generic_category()));
              return;
            }
          }

          if (const auto buffered = std::ssize(r.wire); buffered > 0) {
            INFO("rtsp.net", "buffered", "buffered={}\n", r.wire.size());
            if (auto consumed = aes.decrypt(r.wire, r.packet); consumed != buffered) {

              // incomplete decipher, read from socket
              asio::async_read(sock, r.buffer(), asio::transfer_at_least(1), std::move(*this));
              return;
            }
          }

          INFO("rtsp.net", "consumed", "wire={} packet={}\n", r.wire.size(), r.packet.size());

          // we potentially have a complete message, attempt to find the delimiters
          if (r.find_delims() == false) {
            // we need more data in the packet to continue
            // incomplete decipher, read from socket
            asio::async_read(sock, r.buffer(), asio::transfer_at_least(1), std::move(*this));
            return;
          }

          INFO("rtsp.net", "find_delms", "delims={}\n", r.delims.size());

          if (r.headers.parse_ok == false) {
            // we haven't parsed headers yet, do it now
            auto parse_ok = r.headers.parse(r.packet, r.delims);

            if (parse_ok == false) {
              handler(error_code(errc::bad_message, sys::generic_category()));
              return;
            }
          }

          if (const auto more_bytes = r.populate_content(); more_bytes > 0) {
            asio::async_read(sock, r.buffer(), asio::transfer_exactly(more_bytes),
                             std::move(*this));
            return;
          }

          if (!ec) {
            Saver(Saver::IN, r.headers, r.content);
            Stats::write(stats::RTSP_SESSION_RX_PACKET, std::ssize(r.packet));
          }

          handler(ec); // end of composed operation

        } // end operator()

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
      }; // end struct intermediate_completion_handler

      // initiate the underlying async_read using our intermediate completion handler
      asio::async_read(sock, r.buffer(), asio::transfer_at_least(117),
                       intermediate_completion_handler{
                           sock, // the socket
                           r,    // the rquest
                           aes,
                           std::forward<decltype(completion_handler)>(completion_handler) // handler
                       });
    }; // initiation lambda end

    return asio::async_initiate<CompletionToken, void(error_code)>(
        initiation,     // initiation function object
        token,          // user supplied callback
        std::ref(sock), // the socket to read from
        std::ref(r),    // the request destination
        std::ref(aes)   // the aes context
    );
  }

  /// @brief Write a complete message asuyn
  /// @tparam R
  /// @tparam CompletionToken
  /// @param sock
  /// @param r
  /// @param aes
  /// @param token
  /// @return
  template <typename R, typename CompletionToken>
  static auto async_write_msg(tcp_socket &sock, R &r, Aes &aes, CompletionToken &&token) {

    auto initiation = [](auto &&completion_handler, tcp_socket &sock, R &r, Aes &aes) {
      struct intermediate_handler {
        tcp_socket &sock;
        R &r;
        Aes &aes;
        typename std::decay<decltype(completion_handler)>::type handler;

        void operator()(const error_code &ec, std::size_t bytes) {

          INFO("rtsp.net", "write", "bytes={}\n", bytes);

          Stats::write(stats::RTSP_SESSION_TX_REPLY, static_cast<int64_t>(bytes));

          if (!ec) {
            Saver(Saver::OUT, r.headers_out, r.content_out, r.resp_code);

            handler(ec); // call user-supplied handler}
            return;
          }
        } // end operator

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

      aes.encrypt(r.wire); // NOTE: noop until cipher exchange completed

      // initiate the actual async operation
      asio::async_write( //
          sock, r.buffer(),
          intermediate_handler{
              sock, r, aes,
              std::forward<decltype(completion_handler)>(completion_handler) //
          });
    };

    // initiate the async operation
    return asio::async_initiate<CompletionToken, void(error_code)>(
        initiation, // initiation function object
        token,      // user supplied callback
        std::ref(sock), std::ref<R>(r),
        std::ref(aes)); // move the msg for use within the async operation
  }
};

} // namespace rtsp
} // namespace pierre