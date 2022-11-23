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

#include <algorithm>
#include <optional>
#include <ranges>
#include <set>

namespace pierre {

class SilentFrame : public Frame {
private:
  SilentFrame() noexcept : Frame(frame::DSP_COMPLETE) {

    // lambda since we may need to recalculate
    auto calc_diff = [now = pet::now_monotonic(), this](const auto frame_num) {
      return (epoch + (InputInfo::lead_time * frame_num)) - now;
    };

    Nanos diff = calc_diff(frame_num);

    // silent frames are only READY or FUTURE (never OUTDATED)
    // if oOUTDATED  then there has been a gap in this silence frame
    // sequence so reset the epoch and frame_num and recalculate diff
    // before calling state_now()
    if (diff < Nanos::zero()) {
      reset();

      diff = calc_diff(frame_num);
    }

    state_now(diff);
    ++frame_num;
  }

public:
  static auto create() noexcept { return std::shared_ptr<SilentFrame>(new SilentFrame()); }

  static void reset() noexcept {
    frame_num = 0;
    epoch = pet::now_monotonic();
  }

public:
  virtual Nanos sync_wait_recalc() noexcept final {
    return set_sync_wait(sync_wait() - (Nanos)since_birth);
  }

private:
  Elapsed since_birth; // elapsed time since frame creation, used for recalc

private:
  static Nanos epoch;
  static int64_t frame_num;

public:
  static constexpr csv module_id{"SILENT_FRAME"};
};

} // namespace pierre