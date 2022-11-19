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
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/uint8v.hpp"
#include "frame/racked.hpp"

#include <chrono>

namespace pierre {
namespace airplay {
namespace session {

// constructor notes:
//   1. socket is passed as a reference to a reference so we must move to our local
//      socket reference
//
// asyncAudioBufferLoop notes:
//   1. nothing within this function can be captured by the lamba
//      because the scope of this function ends before
//      the lamba executes
//
//   2. the async_* call will attach the lamba to the io_ctx
//      then immediately return and then this function returns
//
//   3. we capture a shared_ptr (s) for access within the lamba,
//      that shared_ptr is kept in scope while async_read is
//      waiting for data on the socket then during execution
//      of the lamba
//
//   4. when called again from within the lamba the sequence of
//      events repeats (this function returns) and the shared_ptr
//      once again goes out of scope
//
//   5. the crucial point -- we must keep the use count
//      of the session above zero until the session ends
//      (e.g. due to error, natural completion, io_ctx is stopped)
//
// within the lamba:
//
//   1. since this isn't captured all access to member functions/variables
//      must use s
//
//   2. in general, we try to minimize logic in the lamba and instead call
//      member functions to get back within this context
//
//   3. check the socket status frequently and bail out if error
//      (bailing out implies allowing the shared_ptr to go out of scope)
//
//   4. upon receipt of the packet length we check if that many bytes are
//      available (for debug) then async_* them in
//
//   5. if at any point the socket isn't ready fall through and release the
//      shared_ptr
//
//   6. otherwise schedule async_* of next packet
//
//   7. reminder this session will auto destruct if more async_ work
//      isn't scheduled
//
// misc notes:
//   1. the first return of this function traverses back to the Server that created
//      the Audio (in the same io_ctx)
//   2. subsequent returns are to the io_ctx and match the required void return
//      signature

static constexpr size_t PACKET_LEN_BYTES{sizeof(uint16_t)};

void Audio::asyncLoop() {

  packet_len_buffer.clear(); // ensure packet_len_buffer is ready

  // start by reading the packet length
  asio::async_read(                             // async read the length of the packet
      socket,                                   // read from socket
      asio::dynamic_buffer(packet_len_buffer),  // into this buffer
      asio::transfer_exactly(PACKET_LEN_BYTES), // the size of the pending packet
      asio::bind_executor(                      // via a bound executor
          local_strand,                         // of the local stand
          [s = shared_from_this()](error_code ec, size_t rx_bytes) {
            // check for error and ensure receipt of the packet length
            if (s->isReady(ec)) {
              if (rx_bytes == PACKET_LEN_BYTES) {
                INFOX(module_id, "ASYNC_LOOP", "will call read packet_len={}\n", s->packetLength())

                // async load the packet
                s->asyncRxPacket();
              }

            }  // s is about to go out of scope...
          })); // shared_ptr.use_count--
}

void Audio::asyncRxPacket() {
  const auto packet_size = packetLength();

  asio::async_read(                        // async read the rest of the packet
      socket,                              // from this socket
      asio::dynamic_buffer(packet_buffer), // put the bytes read here
      asio::transfer_exactly(packet_size), // size of the rest of the packet
      asio::bind_executor(                 // via
          local_strand,                    // serialize through this strand
          [s = shared_from_this()](error_code ec, size_t rx_bytes) {
            // confirm the socket is ready
            if (s->isReady(ec)) {
              if (rx_bytes == s->packetLength()) {
                uint8v handoff;                       // handoff this packet
                std::swap(handoff, s->packet_buffer); // by swapping local buffer

                Racked::handoff(std::move(handoff));
              } else {
                const auto len = s->packetLength();
                INFO(module_id, "RX_PACKET", "rx_bytes/packet_len mismatch {} != {}\n", rx_bytes,
                     len);
              }

              // wait for next packet
              s->asyncLoop();
            }

            // not ready... fall through and s-destruct
          }));
}

uint16_t Audio::packetLength() {
  // prevent running afoul of strict aliasing rule
  uint16_t len{0};

  len += packet_len_buffer[0] << 8;
  len += packet_len_buffer[1];

  if (len > 2) { // prevent overflow since we're unsigned
    len -= PACKET_LEN_BYTES;
  }

  return len;
}

} // namespace session
} // namespace airplay
} // namespace pierre
