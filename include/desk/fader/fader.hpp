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
  Fader(const Nanos duration) : duration(duration) {}

  bool active() const { return !finished; }
  bool progress(double percent) const { return fader_progress > percent; }
  bool complete() const { return finished; }
  auto frameCount() const { return frames.count; }
  virtual Hsb position() const { return Hsb(); }

  bool travel(); // returns true to continue traveling

protected:
  virtual void doFinish() = 0;
  virtual double doTravel(double total, double current) = 0;
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
};
} // namespace desk
} // namespace pierre
