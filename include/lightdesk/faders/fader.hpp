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

#include <chrono>
#include <memory>

namespace pierre {
namespace lightdesk {
namespace fader {

class Base {
public:
  using usec = std::chrono::microseconds;
  typedef std::chrono::microseconds millis;
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
  bool travel(); // use started_at as reference

protected:
  virtual void handleFinish() = 0;
  virtual float handleTravel(const float total, const float current) = 0;
  virtual float handleTravel(float progress) {
    handleTravel(progress, 1.0);
    return progress;
  }

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
