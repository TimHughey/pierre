/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

class Color : public Base {

public:
  struct Opts {
    lightdesk::Color origin;
    lightdesk::Color dest;
    ulong ms;
  };

public:
  Color(const Opts &opts)
      : Base(opts.ms), _origin(opts.origin), _dest(opts.dest) {}

  const lightdesk::Color &location() const { return _location; }

  // virtual void handleFinish() override { _location = _dest; }
  virtual void handleFinish() override {}
  virtual void handleTravel(const float current, const float total) = 0;

protected:
  lightdesk::Color _origin;
  lightdesk::Color _dest;
  lightdesk::Color _location; // current fader location
};

typedef std::unique_ptr<Color> upColor;

} // namespace fader
} // namespace lightdesk
} // namespace pierre

#endif
