/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#pragma once

#include "base/color.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"

#include <memory>

namespace pierre {

class Fader;
typedef std::unique_ptr<Fader> uqFader;

class Fader {
public:
  Fader(const Nanos duration) : duration(duration){};

  bool active() const { return !finished; }
  bool checkProgress(double percent) const { return progress > percent; }
  bool complete() const { return finished; }
  auto frameCount() const { return frames.count; }
  virtual const Color &position() const { return color::NONE; }
  const Nanos &startdAt() const { return start_at; }

  bool travel(); // returns true to continue traveling

protected:
  virtual void doFinish() = 0;
  virtual float doTravel(const float total, const float current) = 0;
  virtual float doTravel(float progress) {
    doTravel(progress, 1.0);
    return progress;
  }

private:
  // order dependent
  const Nanos duration;

  // order independent
  double progress = 0.0;
  bool finished = false;
  Nanos start_at{0};

  struct {
    uint32_t count{0};
  } frames;
};

} // namespace pierre
