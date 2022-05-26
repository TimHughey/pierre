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
#include "anchor/anchor.hpp"
#include "clock/info.hpp"
#include "conn_info/conn_info.hpp"
#include "packet/basic.hpp"
#include "packet/queued.hpp"

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
using namespace pierre::packet;

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
  // start by reading the packet length
  async_read(socket,                             // read from socket
             Queued::ptr()->lenBuffer(),         // into this buffer
             transfer_exactly(Queued::lenBytes), // the size of the pending packet
             bind_executor(                      // via a bound executor
                 local_strand,                   // of the local stand
                 [self = shared_from_this()](error_code ec, size_t rx_bytes) {
                   // check for error and ensure receipt of the packet length
                   if (self->isReady(ec)) {
                     if (rx_bytes == Queued::PACKET_LEN_BYTES) {
                       const auto len = Queued::ptr()->length();

                       if (false) { // debug
                         auto f = FMT_STRING("{} packet_len={}\n");
                         fmt::print(f, fnName(), len);
                       }

                       // async load the packet
                       self->asyncRxPacket(len); // schedules more work as needed
                       self->ensureRxBytesReport();
                     }
                   }  // self is about to go out of scope...
                 })); // shared_ptr.use_count--
}

void Audio::asyncRxPacket(size_t packet_len) {
  async_read(socket,                                  // the socket of interest
             dynamic_buffer(Queued::ptr()->buffer()), // where to put the data
             transfer_exactly(packet_len - 2),        // how much to rx
             bind_executor(local_strand,              // run on this strand
                           [self = shared_from_this()](error_code ec,
                                                       size_t rx_bytes) { // completion handler
                             // bail out when not reqdy
                             if (self->isReady(ec) == false) // self-destruct
                               return;

                             if (true) { // debug
                               constexpr auto f = FMT_STRING("{} AUDIO RX "
                                                             "sampletime={} "
                                                             "raw={} ns_since_sample={} now={}\n");

                               auto clock_info = Clock::ptr()->info();
                               const int64_t now = Clock::now();

                               const int64_t time_since_sample = now - clock_info.sampleTime;

                               if (false) {
                                 fmt::print(f, runTicks(), clock_info.sampleTime,
                                            clock_info.rawOffset, time_since_sample, now);
                               }
                             }

                             self->accumulate(RX, rx_bytes);       // track stats
                             Queued::ptr()->storePacket(rx_bytes); // notify bytes received
                             self->asyncLoop();                    // async read next packet
                           }));
}

void Audio::ensureRxBytesReport() {
  static std::once_flag flag;

  std::call_once(flag, [&]() { timedRxBytesReport(); });
}

void Audio::teardown() {
  [[maybe_unused]] error_code ec;
  timer.cancel(ec);

  Base::teardown();
}

void Audio::timedRxBytesReport() {
  timer.expires_after(10s);

  timer.                                                   // this timer
      async_wait(                                          // waits via
          bind_executor(                                   // a specific executor that
              local_strand,                                // uses this strand
              [self = shared_from_this()](error_code ec) { // for this completion handler
                static int64_t rx_last = 0;
                // only check for success here, check for actual packet length bytes later
                if (ec == errc::success) {
                  const auto rx_total = self->accumulated(RX);

                  if (rx_total == 0) {
                    self->timedRxBytesReport();
                  }

                  // if (rx_total != rx_last) {
                  constexpr auto f = FMT_STRING("{} {} RX total={:<10} 10s={:<10} "
                                                "seq_num={:<11} {:>11} count={:>6} "
                                                "timest={:<12} {:>12} "
                                                "as_secs={:>6.4}\n");
                  const auto in_10secs = rx_total - rx_last;
                  rx_last = rx_total;

                  // const auto &stream_data = ConnInfo::ptr()->streamData();
                  const auto clock_info = Clock::ptr()->info();

                  const int64_t time_since_sample = clock_info.sampleTime - clock_info.now();
                  const double as_secs = (double)time_since_sample * std::pow(10, -9);

                  auto queued = Queued::ptr();

                  fmt::print(f, runTicks(), self->sessionId(), rx_total, in_10secs,
                             queued->seqFirst(), queued->seqLast(), queued->seqCount(),
                             queued->timestFirst(), queued->timestLast(), as_secs);
                  // }

                  self->timedRxBytesReport();
                }
              }));
}

} // namespace session
} // namespace airplay
} // namespace pierre
