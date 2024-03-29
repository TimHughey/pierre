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

#include "frame/silent_frame.hpp"

#include <optional>

namespace pierre {

// static class data

// since_frame represents when the last frame was generated.  it is used to
// calculate the sync_wait for the next frame to simulate the correct
// frame rate in absence of the master clock and/or anchor

// Elapsed SilentFrame::since_frame; // establish a reference time
// steady_timepoint SilentFrame::epoch{steady_clock::now()};
Nanos SilentFrame::epoch{pet::now_monotonic()};
int64_t SilentFrame::frame_num{0};

} // namespace pierre