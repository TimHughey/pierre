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

#ifndef pierre_lightdesk_fader_color_hpp
#define pierre_lightdesk_fader_color_hpp

#include "lightdesk/color.hpp"
#include "lightdesk/faders/easings.hpp"
#include "lightdesk/faders/fader.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {
namespace fader {

class ColorToColor : public Base {

public:
  struct Opts {
    Color origin;
    Color dest;
    ulong ms;
  };

public:
  ColorToColor(const Opts &opts)
      : Base(opts.ms), _origin(opts.origin), _dest(opts.dest),
        _location(opts.origin) {}

  const lightdesk::Color &location() const { return _location; }

  virtual void handleTravel(const float progress) override = 0;

protected:
  Color _origin;
  Color _dest;
  Color _location; // current fader location
};

typedef std::unique_ptr<ColorToColor> upColorToColor;

} // namespace fader
} // namespace lightdesk
} // namespace pierre

#endif
