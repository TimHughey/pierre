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
#include "base/uint8v.hpp"
#include "conn_info/conn_info.hpp"
#include "player/player.hpp"

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <chrono>
#include <cmath>
#include <fmt/format.h>
#include <mutex>

namespace pierre {
namespace airplay {
namespace session {

using namespace std::chrono_literals;
using namespace boost::asio;
using namespace boost::system;

namespace errc = boost::system::errc;

Audio::Audio(const Inject &di)
    : Base(di, csv("AUDIO SESSION")), // Base holds the newly connected socket
      timer(di.io_ctx)                // rx bytes reporting
{}

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
  static std::once_flag __stats_started;
  packet_len_buffer.clear(); // ensure packet_len_buffer is ready

  // start by reading the packet length
  async_read(                             // async read the length of the packet
      socket,                             // read from socket
      dynamic_buffer(packet_len_buffer),  // into this buffer
      transfer_exactly(PACKET_LEN_BYTES), // the size of the pending packet
      bind_executor(                      // via a bound executor
          local_strand,                   // of the local stand
          [self = shared_from_this()](error_code ec, size_t rx_bytes) {
            // check for error and ensure receipt of the packet length
            if (self->isReady(ec)) {
              if (rx_bytes == PACKET_LEN_BYTES) {
                __LOGX("{} will read packet_len={}\n", fnName(), self->packetLength(););

                // async load the packet
                self->asyncRxPacket();

                std::call_once(__stats_started, [self = self] { self->stats(); });
              }

            }  // self is about to go out of scope...
          })); // shared_ptr.use_count--
}

void Audio::asyncRxPacket() {
  const auto packet_size = packetLength();

  async_read(                        // async read the rest of the packet
      socket,                        // from this socket
      dynamic_buffer(packet_buffer), // put the bytes read here
      transfer_exactly(packet_size), // size of the rest of the packet
      bind_executor(                 // via
          local_strand,              // serialize through this strand
          [self = shared_from_this()](error_code ec, size_t rx_bytes) {
            // confirm the socket is ready
            if (self->isReady(ec)) {
              // track stats
              self->accumulate(RX, rx_bytes);

              if (rx_bytes == self->packetLength()) {
                uint8v packet_handoff;                          // handoff this packet
                std::swap(packet_handoff, self->packet_buffer); // by swapping local buffer

                // inform player a complete packet is ready
                Player::accept(packet_handoff);
              } else {
                const auto len = self->packetLength();
                __LOG0("{} rx_bytes/packet_len mismatch {} != {}\n", fnName(), rx_bytes, len);
              }

              // wait for next packet
              self->asyncLoop();
            }

            // not ready... fall through and self-destruct
          }));
}

uint16_t Audio::packetLength() {
  // prevent running afoul of strict aliasing rule
  uint16_t len = 0;

  len += packet_len_buffer[0] << 8;
  len += packet_len_buffer[1];

  if (len > 2) { // prevent overflow since we're unsigned
    len -= PACKET_LEN_BYTES;
  }

  __LOGX("{} len={}\n", fnName(), len);

  return len;
}

void Audio::teardown() {
  [[maybe_unused]] error_code ec;
  timer.cancel(ec);

  Base::teardown();
}

void Audio::stats() {
  timer.expires_after(10s);

  timer.                                                   // this timer
      async_wait(                                          // waits via
          bind_executor(                                   // a specific executor
              local_strand,                                // running on this strand
              [self = shared_from_this()](error_code ec) { // to execute this completion handler
                static uint64_t rx_last = 0;
                // only check for success here, check for actual packet length bytes later
                if (ec == errc::success) {
                  const auto rx_total = self->accumulated(RX);

                  if (rx_total == 0) {
                    self->stats();
                  }

                  if (rx_total != rx_last) {
                    __LOGX("session_id={:#x} RX total={:<10} 10s={:<10}\n", self->sessionId(),
                           rx_total, (rx_total - (int64_t)rx_last));
                    rx_last = rx_total;
                  }

                  self->stats();
                }
              }));
}

} // namespace session
} // namespace airplay
} // namespace pierre
