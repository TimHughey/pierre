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

#include "session/event.hpp"

#include <algorithm>
#include <fmt/format.h>

namespace pierre {
namespace airplay {
namespace session {

using namespace std::chrono_literals;
using namespace boost::asio;
using namespace boost::system;
using namespace pierre::packet;

void Event::asyncLoop() {
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

  async_read(socket, dynamic_buffer(wire()), transfer_at_least(16),
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
                 self->handleEvent(rx_bytes);

                 if (self->isReady()) {
                   // 3. call nextRequest() to reset buffers and state
                   self->nextEvent();

                   // 4. the socket is still ready, call asyncRequest to await next request
                   // 5. async_read captures a fresh shared_ptr
                   // 6. reminder... call returns when async_read registers the work with io_ctx
                   self->asyncLoop();
                   // 7. falls through
                 }
               } // self is about to go out of scope...
             }); // self is out of scope and the shared_ptr use count is reduced by one

  // final notes:
  // 1. the first return of this function traverses back to the Server that created the
  //    Event (in the same io_ctx)
  // 2. subsequent returns are to the io_ctx and match the required void return signature
}

void Event::handleEvent(size_t rx_bytes) {
  fmt::print("{} bytes={}\n", fnName(), rx_bytes);

  // the following function calls do not contain async_* calls
  accumulate(RX, rx_bytes);

  rxAvailable(); // drain the socket
}

bool Event::rxAvailable() {
  if (isReady()) {
    error_code ec;
    size_t avail_bytes = socket.available(ec);

    auto buff = dynamic_buffer(_wire);

    while (isReady(ec) && (avail_bytes > 0)) {
      auto tx_bytes = read(socket, buff, transfer_at_least(avail_bytes), ec);
      accumulate(RX, tx_bytes);

      avail_bytes = socket.available(ec);
    }
  }

  return isReady();
}

void Event::nextEvent() { _wire.clear(); }

bool Event::rxAtLeast(size_t bytes) {
  if (isReady()) {
    auto buff = dynamic_buffer(_wire);

    error_code ec;
    auto rx_bytes = read(socket, buff, transfer_at_least(bytes), ec);

    accumulate(RX, rx_bytes);
  }
  return isReady();
}

} // namespace session
} // namespace airplay
} // namespace pierre