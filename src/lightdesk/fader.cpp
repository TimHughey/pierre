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

#include <iostream>

#include "lightdesk/fader.hpp"

using namespace std::chrono;

namespace pierre {
namespace lightdesk {

Fader::Fader(const Fader::Opts opts)
    : _opts(opts), _location(opts.origin), _progress(0.0), _finished(false) {

  _duration = usec(_opts.ms * 1000);

  _frames.count = 0;
}

bool Fader::checkProgress(double percent) const {
  auto rc = false;

  if (progress() >= percent) {
    rc = true;
  }

  return rc;
}

bool Fader::travel() {
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
      _location.setBrightness(0);
    } else {
      _progress = (float)elapsed.count() / (float)_duration.count();

      auto brightness = _opts.origin.brightness();
      // auto fade_level = sin((_progress * pi) / 2.0);
      // auto fade_level = 1.0 - pow(1.0 - _progress, 3.0);

      // ease out exponent
      // auto fade_level =
      //     (_progress == 1.0 ? 1.0 : 1.0 - pow(2.0, -10.0 * _progress));

      // ease out quint
      auto fade_level = 1.0 - pow(1.0 - _progress, 5.0);

      // ease out circ
      // auto fade_level = sqrt(1.0 - pow(_progress - 1.0, 2.0));

      _location.setBrightness(brightness - (fade_level * brightness));
    }
  }

  _finished = !more_travel;

  _frames.count++;
  return more_travel;
}

} // namespace lightdesk
} // namespace pierre
