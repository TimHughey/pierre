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

#include "base/dura_t.hpp"
#include "base/types.hpp"
#include "desk/color/hsb.hpp"

namespace pierre {

namespace desk {

class Fader {
public:
  constexpr Fader(const Nanos duration) : duration(duration) {}

  constexpr bool active() const noexcept { return !finished; }
  constexpr bool progress(double percent) const noexcept { return fader_progress > percent; }
  constexpr bool complete() const noexcept { return finished; }
  constexpr auto frameCount() const noexcept { return frames.count; }
  virtual Hsb position() const noexcept { return Hsb(); }

  bool travel(); // returns true to continue traveling

protected:
  virtual void doFinish() {}
  virtual double doTravel([[maybe_unused]] double total, [[maybe_unused]] double current) {
    return 0;
  };
  virtual double doTravel(double progress) {
    doTravel(progress, 1.0);
    return progress;
  }

private:
  // order dependent
  const Nanos duration;

  // order independent
  double fader_progress{0.0};
  bool finished{false};
  Nanos start_at{0};

  struct {
    uint32_t count{0};
  } frames;

public:
  MOD_ID("desk::fader");
};
} // namespace desk
} // namespace pierre
