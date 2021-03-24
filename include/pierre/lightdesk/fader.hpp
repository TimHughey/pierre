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

#include <iostream>

#include "lightdesk/color.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

class Fader {
public:
  struct Opts {
    Color origin;
    Color dest;
    float secs;
  };

public:
  Fader() = default;
  Fader(const Opts opts);

  bool active() const { return !_finished; }
  bool checkProgress(double percent) const;
  bool finished() const { return _finished; }
  const Color &location() const { return _location; }
  double progress() const { return 100.0 - (_progress * 100.0); }

  bool travel();

private:
  Opts _opts;
  Color _location; // current fader location
  bool _traveled = false;
  bool _finished = true;

  uint _frames = 0;

  double _step = 0.0;
  double _progress = 0.0;
};

typedef std::shared_ptr<Fader> spFader;
typedef std::unique_ptr<Fader> upFader;

} // namespace lightdesk
} // namespace pierre

#endif
