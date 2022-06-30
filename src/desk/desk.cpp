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
// #include "desk/fx/majorpeak.hpp"
// #include "desk/fx/silence.hpp"

#include <math.h>
#include <ranges>

namespace ranges = std::ranges;

namespace pierre {
namespace shared {
std::optional<shDesk> __tracker;
std::optional<shDesk> &tracker() { return __tracker; }
} // namespace shared

// static creation, access to instance
shDesk Desk::create() { return shared::tracker().emplace(new Desk()); }
shDesk Desk::ptr() { return shared::tracker().value()->shared_from_this(); }

// Desk::Desk(shared_ptr<audio::Dsp> dsp) : _dsp(std::move(dsp)) {
//   Fx::setTracker(_tracker);

//   _tracker->insert<PinSpot>("main", 1);
//   _tracker->insert<PinSpot>("fill", 7);
//   _tracker->insert<DiscoBall>("discoball", 1);
//   _tracker->insert<ElWire>("el dance", 2);
//   _tracker->insert<ElWire>("el entry", 3);
//   _tracker->insert<LedForest>("led forest", 4);

//   main = _tracker->unit<PinSpot>("main");
//   fill = _tracker->unit<PinSpot>("fill");
//   led_forest = _tracker->unit<LedForest>("led forest");
//   el_dance_floor = _tracker->unit<ElWire>("el dance");
//   el_entry = _tracker->unit<ElWire>("el entry");
//   discoball = _tracker->unit<DiscoBall>("discoball");

//   // lightdesk always starts assuming silence
//   _active.fx = make_shared<Silence>();
// }

// Desk::~Desk() { Fx::resetTracker(); }

// void Desk::executeFx() {
//   audio::spPeaks peaks = _dsp->peaks();

//   _active.fx->execute(peaks);

//   // this is placeholder code for future Fx
//   // at present the only Fx is MajorPeak which never ends
//   if (_active.fx->finished()) {
//     _active.fx = make_shared<MajorPeak>();
//   }
// }

// void Desk::leave() {
//   // auto x = State::leavingDuration<seconds>().count();

//   {
//     lock_guard lck(_active.mtx);
//     _active.fx = make_shared<Leave>();
//   }

//   // cout << "leaving for " << x << " second";
//   // if (x > 1) {
//   //   cout << "s";
//   // }

//   // cout << " (or Ctrl-C to quit immediately)";
//   // cout.flush();

//   this_thread::sleep_for(State::leavingDuration<milliseconds>());

//   State::shutdown();

//   // cout << endl;
// }

// void Desk::stream() {
//   while (State::isRunning()) {
//     // nominal condition:
//     // sound is present and MajorPeak is active
//     if (_active.fx->matchName("MajorPeak"sv)) {
//       // if silent but not yet reached suspended start Leave
//       if (State::isSilent()) {
//         if (!State::isSuspended()) {
//           _active.fx = make_shared<Leave>();
//         }
//       }

//       goto delay;
//     }

//     // catch anytime we're coming out of silence or suspend
//     // ensure that MajorPeak is running when not silent and not suspended
//     if (!State::isSilent() && !State::isSuspended()) {
//       _active.fx = make_shared<MajorPeak>();
//       goto delay;
//     }

//     // transitional condition:
//     // there is no sound present however the timer for suspend hasn't elapsed
//     if (_active.fx->matchName("Leave"sv)) {
//       // if silent and have reached suspended, start Silence
//       if (State::isSilent() && State::isSuspended()) {
//         _active.fx = make_shared<Silence>();
//       }
//     }

//     goto delay;

//   delay:
//     this_thread::sleep_for(seconds(1));
//   }
// }

} // namespace pierre
