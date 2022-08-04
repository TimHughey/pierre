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
#include "desk/fx/majorpeak.hpp"
#include "desk/fx/silence.hpp"
#include "desk/msg.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/opts.hpp"
#include "mdns/mdns.hpp"

#include <math.h>
#include <ranges>

namespace pierre {
namespace shared {
std::optional<shDesk> __desk;
} // namespace shared

// must be defined in .cpp to hide mdns
Desk::Desk(io_context &io_ctx)
    : io_ctx(io_ctx),                        // io_ctx run by AirPlay 2 controller
      local_strand(io_ctx),                  // local strand to serialize work
      active_fx(createFX<fx::Silence>()),    // active FX initializes to Silence
      zservice(mDNS::zservice("_ruth._tcp")) // get the remote service, if available
{}

// static creation, access to instance
shDesk Desk::create(io_context &io_ctx) {
  auto desk = shared::desk().emplace(new Desk(io_ctx));

  desk->addUnit<PinSpot>(unit::MAIN_SPOT_OPTS);
  desk->addUnit<PinSpot>(unit::FILL_SPOT_OPTS);
  desk->addUnit<DiscoBall>(unit::DISCO_BALL_OPTS);
  desk->addUnit<ElWire>(unit::EL_DANCE_OPTS);
  desk->addUnit<ElWire>(unit::EL_ENTRY_OPTS);
  desk->addUnit<LedForest>(unit::LED_FOREST_OPTS);

  return desk;
}

// General API
void Desk::handlePeaks(const PeakInfo &peak_info) {
  auto msg = desk::Msg::create(peak_info);

  if ((active_fx->matchName(fx::SILENCE)) && !peak_info.silence) {
    activateFX<fx::MajorPeak>(fx::MAJOR_PEAK);
  }

  ranges::for_each(units, [](shHeadUnit unit) { unit->preExecute(); });

  active_fx->executeLoop(peak_info.left);

  ranges::for_each(units, [msg = msg->ptr()](shHeadUnit unit) { unit->updateMsg(msg); });

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
