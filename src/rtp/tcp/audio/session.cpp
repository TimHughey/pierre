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
#include <chrono>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <regex>
#include <source_location>
#include <string_view>
#include <thread>

#include "rtp/tcp/audio/packet.hpp"
#include "rtp/tcp/audio/session.hpp"

namespace pierre {
namespace rtp {
namespace tcp {

using namespace std::chrono_literals;
using namespace boost::asio;
using namespace boost::system;
using namespace pierre::packet;

using error_code = boost::system::error_code;
using src_loc = std::source_location;
using string_view = std::string_view;

/*
constructor notes:
  1. socket is passed as a reference to a reference so we must move to our local socket reference

asyncAudioBufferLoop notes:
  1. nothing within this function can be captured by the lamba
     because the scope of this function ends before
     the lamba executes

  2. the async_* call will attach the lamba to the io_ctx
     then immediately return and then this function returns

  3. we capture a shared_ptr (self) for access within the lamba,
     that shared_ptr is kept in scope while async_read is
     waiting for data on the socket then during execution
     of the lamba

  4. when called again from within the lamba the sequence of
     events repeats (this function returns) and the shared_ptr
     once again goes out of scope

  5. the crucial point -- we must keep the use count
     of the session above zero until the session ends
     (e.g. due to error, natural completion, io_ctx is stopped)

within the lamba:

  1. since this isn't captured all access to member functions/variables
     must use self

  2. in general, we try to minimize logic in the lamba and instead call
     member functions to get back within this context

  3. check the socket status frequently and bail out if error
     (bailing out implies allowing the shared_ptr to go out of scope)

  4. upon receipt of the packet length we check if that many bytes are
     available (for debug) then async_* them in

  5. if at any point the socket isn't ready fall through and release the
     shared_ptr

  6. otherwise schedule async_* of next packet

  7. reminder this session will auto destruct if more async_ work isn't scheduled

misc notes:
  1. the first return of this function traverses back to the Server that created the
     AudioSession (in the same io_ctx)
  2. subsequent returns are to the io_ctx and match the required void return signature
*/
AudioSession::AudioSession(const Opts &opts)
    : socket(std::move(opts.new_socket)),
      anchor(opts.anchor), // io_context / socket for this session
      wire(opts.audio_raw) // queued audio raw data
{}

void AudioSession::asyncAudioBufferLoop() {
  // auto buf = dynamic_buffer(wire.buffer());
  // async_read(socket, buf, transfer_exactly(wire.lengthBytes()),

  if (isReady() == false) // bail out if the socket isn't ready
    return;

  nextAudioBuffer(); // prepare for next audio buffer

  // start by reading the packet length
  auto len_buff = wire.lengthBuffer();
  socket.async_read_some(len_buff, [self = shared_from_this()](error_code ec, size_t rx_bytes) {
    // check for error and ensure receipt of the packet length
    if (self->isReady(ec)) {
      if (rx_bytes == Queued::PACKET_LEN_BYTES) {
        const auto len = self->wire.length();

        if (false) { // new scope introduced for easy IDE folding
          auto f = FMT_STRING("{} packet_len={}\n");
          fmt::print(f, fnName(), len);
        }

        // async load the packet
        self->asyncRxPacket(len); // schedules more work as needed
      }
    } // self is about to go out of scope...
  }); // shared_ptr.use_count--
}

void AudioSession::asyncReportRxBytes(int64_t rx_bytes) {
  timer->expires_after(30s);

  // inject the current _rx_bytes (as rx_last) for compare when the timer expires
  timer->async_wait([self = shared_from_this(), rx_last = rx_bytes](error_code ec) {
    // only check for success here, check for actual packet length bytes later
    if (ec == errc::success) {
      const auto rx_total = self->_rx_bytes;

      if (rx_total != rx_last) {
        const auto in_30secs = rx_total - rx_last;
        fmt::print(FMT_STRING("{} RX total={:<15} 30s={:<6}\n"), fnName(), rx_total, in_30secs);
      }

      self->asyncReportRxBytes(rx_total);
    } else {
      self->timer.reset();
    }
  });
}

void AudioSession::asyncRxPacket(size_t packet_len) {
  [[maybe_unused]] const auto avail = socket.available();

  // if (avail < packet_len) {
  //   const auto f = FMT_STRING("{} avail={:>6} packet_len={:>6}\n");
  //   fmt::print(f, fnName(), avail, packet_len);
  // }

  async_read(socket, dynamic_buffer(wire.buffer()), transfer_exactly(packet_len - 2),
             [self = shared_from_this()](error_code ec, size_t rx_bytes) {
               if (self->isReady(ec)) {
                 self->accumulate(RX, rx_bytes); // track stats
                 self->wire.gotBytes(rx_bytes);  // notify bytes received
                 self->asyncAudioBufferLoop();   // schedule next packet
               }
             });
  // will auto destruct if more work hasn't been scheduled
}

bool AudioSession::isReady(const error_code &ec, const src_loc loc) {
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

void AudioSession::nextAudioBuffer() { wire.reset(); }

/*
bool AudioSession::rxAtLeast(size_t bytes) {
  if (isReady()) {
    auto buff = dynamic_buffer(wire);

    error_code ec;
    auto rx_bytes = read(socket, buff, transfer_at_least(bytes), ec);

    accumulate(Accumulate::RX, rx_bytes);
  }
  return isReady();
}

*/
void AudioSession::accumulate(Accumulate type, size_t bytes) {
  switch (type) {
    case RX:
      _rx_bytes += bytes;
      break;

    case TX:
      _tx_bytes += bytes;
      break;
  }

  if (timer.has_value() == false) {
    timer.emplace(high_resolution_timer(socket.get_executor()));

    // pass the accumulated rx bytes for comparison at timer expire
    asyncReportRxBytes(_rx_bytes);
  }
}

void AudioSession::teardown() {
  [[maybe_unused]] error_code ec;

  socket.cancel(ec);

  socket.shutdown(socket_base::shutdown_both, ec);
  socket.close(ec);
}

} // namespace tcp
} // namespace rtp
} // namespace pierre
