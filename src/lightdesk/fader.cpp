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

#include "lightdesk/fader.hpp"

namespace pierre {
namespace lightdesk {

Fader::Fader(const Fader::Opts opts)
    : _opts(opts), _location(opts.origin), _traveled(false), _finished(false) {
  _step = 1.0 / (_opts.secs * 44.0);
}

bool Fader::checkProgress(double percent) const {
  auto rc = false;

  if (progress() >= percent) {
    rc = true;
  }

  return rc;
}

bool Fader::travel() {
  constexpr double pi = 3.14159265358979323846;
  bool more_travel = false;

  if (_traveled == false) {
    // when use_origin is set start the first travel is to the origin
    // so _location is unchanged
    more_travel = true;
  } else {
    // if we've already traveled once then we move from _location

    if (_progress >= 1.0) {
      more_travel = false;
      _location.setBrightness(0);
    } else {

      auto brightness = _opts.origin.brightness();
      auto fade_level = sin((_progress * pi) / 2.0);
      // auto fade_level = 1.0 - pow(1.0 - _progress, 3.0);
      // auto fade_level =
      //     (_progress == 1.0 ? 1.0 : 1.0 - pow(2, -10 * _progress));

      _location.setBrightness(brightness - (fade_level * brightness));
      _progress += _step;

      more_travel = true;
    }

    _finished = !more_travel;
  }

  _frames++;
  _traveled = true;

  return more_travel;
}

} // namespace lightdesk
} // namespace pierre
