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

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <regex>
#include <source_location>
#include <string_view>
#include <thread>

#include "rtp/audio/buffered/raw.hpp"
#include "rtp/audio/buffered/session.hpp"

namespace pierre {
namespace rtp {
namespace audio {
namespace buffered {

using namespace boost::asio;
using namespace boost::system;
// using namespace pierre::rtp;
using error_code = boost::system::error_code;
using src_loc = std::source_location;
using string_view = std::string_view;

// notes:
//  1. socket is passed as a reference to a reference so we must move to our local socket reference
// Session::Session(tcp_socket &&new_socket)
//     : socket(std::move(new_socket)) // io_context / socket for this session
// {
//   // _wire.resize(input_info.wantFrames(50));
// }

Session::Session(const Opts &opts)
    : socket(std::move(opts.new_socket)),
      anchor(opts.anchor) // io_context / socket for this session
{
  // _wire.resize(input_info.wantFrames(50));
}

void Session::asyncAudioBufferLoop() {
  // notes:
  //  1. nothing within this function can be captured by the lamba
  //     because the scope of this function ends before
  //     the lamba executes
  //
  //  2. the async_read call will attach the lamba to the io_ctx
  //     then immediately return and then this function returns
  //
  //  3. we capture a shared_ptr (self) for access within the lamba,
  //     that shared_ptr is kept in scope while async_read is
  //     waiting for data on the socket then during execution
  //     of the lamba
  //
  //  4. when called again from within the lamba the sequence of
  //     events repeats (this function returns) and the shared_ptr
  //     once again goes out of scope
  //
  //  5. the crucial point -- we must keep the use count
  //     of the session above zero until the session ends
  //     (e.g. due to error, natural completion, io_ctx is stopped)

  async_read(socket, dynamic_buffer(wire()), boost::asio::transfer_exactly(2048),
             [self = shared_from_this()](error_code ec, size_t rx_bytes) {
               // general notes:
               //
               // 1. this was not captured so the lamba is not in 'this' context
               // 2. all calls to the session must be via self (which was captured)
               // 3. we do minimal activities to quickly get to 'this' context

               // essential activies:
               //
               // 1. check the error code
               if (self->isReady(ec)) {
                 // 2. handle the request (remember the data received is in the wire buffer)
                 //    handleRequest will perform the request/reply transaction (serially)
                 //    and returns when the transaction completes or errors
                 self->handleAudioBuffer(rx_bytes);

                 if (self->isReady()) {
                   // 3. call nextRequest() to reset buffers and state
                   self->nextAudioBuffer();

                   // 4. the socket is still ready, call asyncRequest to await next request
                   // 5. async_read captures a fresh shared_ptr
                   // 6. reminder... call returns when async_read registers the work with io_ctx
                   self->asyncAudioBufferLoop();
                   // 7. falls through
                 }
               } // self is about to go out of scope...
             }); // self is out of scope and the shared_ptr use count is reduced by one

  // final notes:
  // 1. the first return of this function traverses back to the Server that created the
  //    Session (in the same io_ctx)
  // 2. subsequent returns are to the io_ctx and match the required void return signature
}

void Session::handleAudioBuffer(size_t rx_bytes) {
  // the following function calls do not contain async_* calls
  uint8_t type = _wire[1] & ~0x80;

  if ((type == 0x60) || (type == 0x56)) {
    fmt::print("type={:#x}\n", type);
  }

  // const auto seq_no = _wire[1] * (1 << 16) + _wire[2] * (1 << 8) + _wire[3];
  // const auto timestamp = nctohl(&packet[4]);

  // fmt::print(fmt_str, fnName(), seq_no);
  accumulate(Accumulate::RX, rx_bytes);

  // rxAvailable(); // drain the socket

  // ensureAllContent(); // load additional content

  // try {
  //   createAndSendReply();
  // } catch (const std::exception &e) {
  //   fmt::print("{} create reply failed: {}\n", fnName(), e.what());

  //   _headers.dump();
  //   _content.dump();
  // }
}

// size_t Session::decrypt(packet::In &packet) {
//   auto consumed = aes_ctx->decrypt(packet, _wire);

//   // fmt::print("{} packet_size={}\n", fnName(), packet.size());
//   _wire.clear(); // wire bytes have been deciphered, clear them

//   return consumed;
// }

bool Session::rxAvailable() {
  if (isReady()) {
    error_code ec;
    size_t avail_bytes = socket.available(ec);

    auto buff = dynamic_buffer(_wire);

    while (isReady(ec) && (avail_bytes > 0)) {
      auto tx_bytes = read(socket, buff, transfer_at_least(avail_bytes), ec);
      accumulate(Accumulate::TX, tx_bytes);

      avail_bytes = socket.available(ec);
    }
  }

  return isReady();
}

// void Session::ensureAllContent() {
//   // bail out if the socket isn't ready
//   if (isReady() == false) {
//     return;
//   }
//
//   // if subsequent data is loaded it will all need up in this packet
//   auto packet = packet::In();
//
//   // decrypt the wire bytes resceived thus far
//   [[maybe_unused]] auto consumed = decrypt(packet);
//
//   // need more content? send Continue reply
//   auto more_bytes = _headers.loadMore(packet.view(), _content);
//
//   // if more bytes are needed, reply Continue
//   if (more_bytes) {
//     constexpr auto CONTINUE = "CONTINUE";
//     auto reply = Reply::create(
//         {.method = CONTINUE, .path = path(), .content = content(), .headers = headers()});
//
//     auto &reply_packet = reply->build();
//
//     aes_ctx->encrypt(reply_packet); // NOTE: noop until cipher exchange completed
//     auto buff = const_buffer(reply_packet.data(), reply_packet.size());
//
//     error_code ec;
//     auto tx_bytes = write(socket, buff, ec);
//     accumulateTx(tx_bytes);
//   }
//
//   // if wire bytes are loaded remember this is an entirely new packet
//   while (more_bytes && isReady()) {
//     if (rxAtLeast(more_bytes)) {
//       [[maybe_unused]] auto consumed = decrypt(packet);
//       more_bytes = _headers.loadMore(packet.view(), _content);
//     }
//   }
// }
//
// void Session::createAndSendReply() {
//   if (isReady() == false) {
//     return;
//   }
//
//   // create the reply to the request
//   auto reply = Reply::create({.method = method(),
//                               .path = path(),
//                               .content = content(),
//                               .headers = headers(),
//                               .host = host->getSelf(),
//                               .service = service->getSelf(),
//                               .aes_ctx = aes_ctx,
//                               .mdns = mdns->getSelf(),
//                               .nptp = nptp->getSelf(),
//                               .rtp = rtp->getSelf()});
//
//   auto &reply_packet = reply->build();
//
//   if (reply->debug()) {
//     auto prefix = std::string_view("SESSION");
//     fmt::print("{} reply seq={:03} method={} path={} resp_code={}\n", prefix, reply->sequence(),
//                method(), path(), reply->responseCodeView());
//   }
//
//   // only send the reply packet if there's data
//   if (reply_packet.size() > 0) {
//     aes_ctx->encrypt(reply_packet); // NOTE: noop until cipher exchange completed
//     auto buff = const_buffer(reply_packet.data(), reply_packet.size());
//
//     error_code ec;
//     auto tx_bytes = write(socket, buff, ec);
//     accumulateTx(tx_bytes);
//
//     // check the most recent ec
//     isReady(ec);
//   }
// }

bool Session::isReady(const error_code &ec, const src_loc loc) {
  auto rc = isReady();

  if (rc) {
    switch (ec.value()) {
      case errc::success:
        break;

      case errc::operation_canceled:
      case errc::resource_unavailable_try_again:
      case errc::no_such_file_or_directory:
        rc = false;
        break;

      default:
        fmt::print("{} SHUTDOWN socket={} err_value={} msg={}\n", loc.function_name(),
                   socket.native_handle(), ec.value(), ec.message());

        socket.shutdown(tcp_socket::shutdown_both);
        socket.close();
        rc = false;
    }
  }

  return rc;
}

void Session::nextAudioBuffer() {
  _wire.clear();
  // _headers.clear();
  // _content.clear();
}

bool Session::rxAtLeast(size_t bytes) {
  if (isReady()) {
    auto buff = dynamic_buffer(_wire);

    error_code ec;
    auto rx_bytes = read(socket, buff, transfer_at_least(bytes), ec);

    accumulate(Accumulate::TX, rx_bytes);
  }
  return isReady();
}

void Session::accumulate(Accumulate type, size_t bytes) {
  switch (type) {
    case RX:
      _rx_bytes += bytes;
      break;

    case TX:
      _tx_bytes += bytes;
      break;
  }
}

void Session::teardown() {
  [[maybe_unused]] error_code ec;

  socket.cancel(ec);

  socket.shutdown(socket_base::shutdown_both, ec);
  socket.close(ec);
}

void Session::dump([[maybe_unused]] DumpKind dump_type) {
  // std::time_t now = std::time(nullptr);
  // std::string dt = std::ctime(&now);

  // if (dump_type == HeadersOnly) {
  //   fmt::print("method={} path={} protocol={} {}\n", method(), path(), protocol(), dt);

  //   _headers.dump();
  // }

  // if (dump_type == ContentOnly) {
  //   _content.dump();
  // }

  // if (dump_type == RawOnly) {
  //   const std::string_view out((const char *)_wire.data(), _wire.size());
  //   fmt::print("{}\n", out);
  // }

  // fmt::print("\n");
}

// int frame_to_ptp_local_time(uint32_t timestamp, uint64_t *time, rtsp_conn_info *conn) {
//   int result = -1;
//   uint32_t anchor_rtptime = 0;
//   uint64_t anchor_local_time = 0;
//   if (get_ptp_anchor_local_time_info(conn, &anchor_rtptime, &anchor_local_time) == clock_ok) {
//     int32_t frame_difference = timestamp - anchor_rtptime;
//     int64_t time_difference = frame_difference;
//     time_difference = time_difference * 1000000000;
//     if (conn->input_rate == 0)
//       die("conn->input_rate is zero!");
//     time_difference = time_difference / conn->input_rate;
//     uint64_t ltime = anchor_local_time + time_difference;
//     *time = ltime;
//     result = 0;
//   } else {
//     debug(3, "frame_to_local_time can't get anchor local time information");
//   }
//   return result;
// }

} // namespace buffered
} // namespace audio
} // namespace rtp
} // namespace pierre
