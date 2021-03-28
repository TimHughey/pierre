/*
    devs/pinspot/fader.hpp - Ruth Pin Spot Fader Action
    Copyright (C) 2020  Tim Hughey

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

#ifndef pierre_lightdesk_fader_hpp
#define pierre_lightdesk_fader_hpp

#include <chrono>

#include "local/types.hpp"

namespace pierre {
namespace lightdesk {
namespace fader {

class Base {
  typedef std::chrono::microseconds usec;
  typedef std::chrono::steady_clock clock;
  typedef std::chrono::time_point<clock> time_point;

public:
  Base(long ms);
  ~Base() = default;

  bool active() const { return !_finished; }
  bool checkProgress(double percent) const;
  bool finished() const { return _finished; }
  size_t frameCount() const { return _frames.count; }
  float progress() const { return _progress; }

  const time_point &startedAt() const { return _started_at; }
  bool travel();

protected:
  virtual void handleFinish() = 0;
  virtual void handleTravel(float progress) = 0;

private:
  float _progress = 0.0;
  bool _finished = false;
  time_point _started_at;
  usec _duration;

  struct {
    size_t count;
  } _frames;
};

typedef std::unique_ptr<Base> upFader;

} // namespace fader
} // namespace lightdesk
} // namespace pierre

#endif
