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

#include "lightdesk/faders/fader.hpp"

using namespace std::chrono;

namespace pierre {
namespace lightdesk {
namespace fader {

Base::Base(long ms) {
  _duration = duration_cast<usec>(milliseconds(ms));

  _frames.count = 0;
}

bool Base::checkProgress(double percent) const {
  auto rc = false;

  if (progress() >= percent) {
    rc = true;
  }

  return rc;
}

bool Base::travel() {
  bool more_travel = true;

  if (_progress == 0.0) {
    // the first invocation (frame 0) represents the origin and start time
    // of the fader
    _started_at = clock::now();
    _progress = 0.0001;

  } else {

    auto elapsed = duration_cast<usec>(clock::now() - _started_at);

    if ((elapsed + _fuzz) >= _duration) {
      more_travel = false;
      handleFinish();
    } else {
      _progress = (float)elapsed.count() / (float)_duration.count();

      handleTravel(_progress);
    }
  }

  _finished = !more_travel;

  _frames.count++;
  return more_travel;
}
} // namespace fader

} // namespace lightdesk
} // namespace pierre
