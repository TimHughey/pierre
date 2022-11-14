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

#pragma once

#include "frame/frame.hpp"
#include "frame/state.hpp"

#include <optional>

namespace pierre {

class SilentFrame : public Frame {
private:
  SilentFrame() noexcept : Frame(frame::DSP_COMPLETE) {

    // rationalize since_frame, more than InputInfo::lead_time
    // has elapsed then reset it to create a READY frame immediately
    if (since_frame() > InputInfo::lead_time) since_frame.reset();

    // we want this frame to render at the correct frame rate
    // so subtract since_frame() to ensure it falls within
    // the lead time window
    state_now(InputInfo::lead_time - since_frame());

    // a frame was generated, reset since_frame
    since_frame.reset();
  }

public:
  static auto create() { return std::shared_ptr<SilentFrame>(new SilentFrame()); }

public:
  virtual Nanos sync_wait_recalc() noexcept override {

    return set_sync_wait(sync_wait() - since_birth());
  }

private:
  Elapsed since_birth; // elapsed time since frame creation, used for recalc

private:
  static Elapsed since_frame;
};

} // namespace pierre