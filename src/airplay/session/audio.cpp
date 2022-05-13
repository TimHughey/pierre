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

#include "session/audio.hpp"
#include "packet/basic.hpp"

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <chrono>
#include <fmt/format.h>

namespace pierre {
namespace airplay {
namespace session {

using namespace std::chrono_literals;
using namespace boost::asio;
using namespace boost::system;
using namespace pierre::packet;

namespace errc = boost::system::errc;

Audio::~Audio() {
  constexpr auto f = FMT_STRING("{} {} shutdown handle={}\n");
  fmt::print(f, runTicks(), "AUDIO SESSION", socket.native_handle());

  [[maybe_unused]] error_code ec;

  // must use error_code overload to prevent throws
  socket.shutdown(socket_base::shutdown_both, ec);
  socket.close(ec);
}

/*
constructor notes:
  1. socket is passed as a reference to a reference so we must move to our local socket
reference

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
     Audio (in the same io_ctx)
  2. subsequent returns are to the io_ctx and match the required void return signature
*/

void Audio::asyncLoop() {
  prepNextPacket();

  // start by reading the packet length
  auto buff = wire.lenBuffer();

  socket.async_read_some(buff, [self = shared_from_this()](error_code ec, size_t rx_bytes) {
    // check for error and ensure receipt of the packet length
    if (self->isReady(ec)) {
      if (rx_bytes == Queued::PACKET_LEN_BYTES) {
        const auto len = self->wire.length();

        if (false) { // debug
          auto f = FMT_STRING("{} packet_len={}\n");
          fmt::print(f, fnName(), len);
        }

        // async load the packet
        self->asyncRxPacket(len); // schedules more work as needed
      }
    } // self is about to go out of scope...
  }); // shared_ptr.use_count--
} // namespace boost::asio::error::voidAudio::asyncLoop()

void Audio::asyncReportRxBytes(int64_t rx_bytes) {
  timer->expires_after(10s);

  // inject the current _rx_bytes (as rx_last) for compare when the timer expires
  timer->async_wait([self = shared_from_this(), rx_last = rx_bytes](error_code ec) {
    // only check for success here, check for actual packet length bytes later
    if (ec == errc::success) {
      const auto rx_total = self->_rx_bytes;

      if (rx_total != rx_last) {
        constexpr auto f = FMT_STRING("{} AUDIO RX total={:<15} 30s={}\n");
        const auto in_30secs = rx_total - rx_last;
        fmt::print(f, runTicks(), rx_total, in_30secs);
      }

      self->asyncReportRxBytes(rx_total);
    } else {
      self->timer.reset();
    }
  });
}

void Audio::asyncRxPacket(size_t packet_len) {
  async_read(socket, dynamic_buffer(wire.buffer()), transfer_exactly(packet_len - 2),
             [self = shared_from_this()](error_code ec, size_t rx_bytes) {
               // bail out when not reqdy
               if (self->isReady(ec) == false) // self-destruct
                 return;

               self->accumulate(RX, rx_bytes);   // track stats
               self->wire.storePacket(rx_bytes); // notify bytes received
               self->asyncLoop();                // async read next packet
             });
}

bool Audio::isReady(const error_code &ec, [[maybe_unused]] csrc_loc loc) {
  auto rc = true;

  if (ec) {
    switch (ec.value()) {
      case errc::success:
        break;

      case errc::operation_canceled:
      case errc::resource_unavailable_try_again:
      case errc::no_such_file_or_directory:
      default: {
        constexpr auto f = FMT_STRING("{} AUDIO SESSION SHUTDOWN socket={} err_value={} msg={}\n");
        fmt::print(f, runTicks(), socket.native_handle(), ec.value(), ec.message());

        rc = false; // return false, object will auto destruct
      }
    }
  }

  return rc;
}

void Audio::prepNextPacket() noexcept { wire.reset(); }

void Audio::accumulate(Accumulate type, size_t bytes) {
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

void Audio::teardown() {
  [[maybe_unused]] error_code ec;

  socket.cancel(ec);
}

} // namespace session
} // namespace airplay
} // namespace pierre
