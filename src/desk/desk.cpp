/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "desk/desk.hpp"
// #include "desk/fx/colorbars.hpp"
// #include "desk/fx/leave.hpp"
#include "desk/fx.hpp"
#include "desk/fx/majorpeak.hpp"
#include "desk/fx/silence.hpp"
#include "desk/msg.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/opts.hpp"
#include "mdns/mdns.hpp"

#include <latch>
#include <math.h>
#include <ranges>

namespace pierre {
namespace shared {
std::optional<shDesk> __desk;
} // namespace shared

// must be defined in .cpp to hide mdns
Desk::Desk()
    : local_strand(io_ctx),                         // local strand to serialize work
      active_fx(fx_factory::create<fx::Silence>()), // active FX initializes to Silence
      zservice(mDNS::zservice("_ruth._tcp")),       // get the remote service, if available
      guard(std::make_shared<work_guard>(io_ctx.get_executor())) {}

// static creation, access to instance
void Desk::init() { // static
  auto desk = shared::desk().emplace(new Desk());

  std::latch threads_latch(DESK_THREADS);
  desk->thread_main = Thread([=, &threads_latch](std::stop_token token) {
    desk->stop_token = token;
    name_thread("Desk", 0);

    // note: work guard created by constructor
    for (auto n = 0; n < DESK_THREADS; n++) {
      desk->threads.emplace_back([=, &threads_latch] {
        name_thread("Desk", n + 1); // main thread is 0
        threads_latch.count_down();
        desk->io_ctx.run();
      });
    }

    desk->io_ctx.run();
  });

  threads_latch.wait(); // wait for all threads to start
}

// General API
void Desk::handle_frame(shFrame frame) {
  auto msg = desk::Msg::create(frame);

  if ((active_fx->matchName(fx::SILENCE)) && !frame->silence()) {
    active_fx = fx_factory::create<fx::MajorPeak>();
  }

  active_fx->render(frame, msg);

  // the socket is only created if it doesn't exist
  // connect() returns true when the connection is ready
  if (connect()) {
    msg->transmit2(local_strand, socket2.value());
  }
}

/*
void Desk::leave() {
  // auto x = State::leavingDuration<seconds>().count();
  {
    lock_guard lck(_active.mtx);
    _active.fx = make_shared<Leave>();
  }

  this_thread::sleep_for(State::leavingDuration<milliseconds>());
  State::shutdown();
}

void Desk::stream() {
  while (State::isRunning()) {
    // nominal condition:
    // sound is present and MajorPeak is active
    if (_active.fx->matchName("MajorPeak"sv)) {
      // if silent but not yet reached suspended start Leave
      if (State::isSilent()) {
        if (!State::isSuspended()) {
          _active.fx = make_shared<Leave>();
        }
      }
      goto delay;
    }
    // catch anytime we're coming out of silence or suspend
    // ensure that MajorPeak is running when not silent and not suspended
    if (!State::isSilent() && !State::isSuspended()) {
      _active.fx = make_shared<MajorPeak>();
      goto delay;
    }
    // transitional condition:
    // there is no sound present however the timer for suspend hasn't elapsed
    if (_active.fx->matchName("Leave"sv)) {
      // if silent and have reached suspended, start Silence
      if (State::isSilent() && State::isSuspended()) {
        _active.fx = make_shared<Silence>();
      }
    }

    goto delay;

  delay:
    this_thread::sleep_for(seconds(1));
  }
}
*/

} // namespace pierre
