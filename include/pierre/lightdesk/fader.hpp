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

#ifndef pierre_pinspot_fader_hpp
#define pierre_pinspot_fader_hpp

#include <chrono>

#include "lightdesk/color.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

class Fader {
  typedef std::chrono::microseconds usec;
  typedef std::chrono::steady_clock clock;
  typedef std::chrono::time_point<clock> time_point;

public:
  struct Opts {
    Color origin;
    Color dest;
    long ms;
  };

public:
  Fader() = default;
  Fader(const Opts opts);

  bool active() const { return !_finished; }
  bool checkProgress(double percent) const;
  bool finished() const { return _finished; }
  const Color &location() const { return _location; }
  float progress() const { return _progress; }

  bool travel();

private:
  Opts _opts;
  Color _location; // current fader location
  float _progress = 0.0;
  bool _finished = true;

  time_point _started_at;

  const usec _fuzz = usec(22000);
  usec _duration;

  struct {
    double count;
  } _frames;

  static constexpr double pi = 3.14159265358979323846;
};

typedef std::shared_ptr<Fader> spFader;
typedef std::unique_ptr<Fader> upFader;

} // namespace lightdesk
} // namespace pierre

#endif
