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

#include "lightdesk/fx/leave.hpp"

namespace pierre
{
  namespace lightdesk
  {
    namespace fx
    {

      Leave::Leave(const float hue_step, const float brightness)
          : _hue_step(hue_step), _brightness(brightness),
            _next_color({.hue = 0, .sat = 100, .bri = _brightness})
      {
        main = unit<PinSpot>("main");
        fill = unit<PinSpot>("fill");
      }

      void Leave::executeFx(audio::spPeaks peaks)
      {
        peaks.reset(); // no use for peaks, release them

        if (_next_brightness < 50.0)
        {
          _next_brightness++;
          _next_color.setBrightness(_next_brightness);
        }

        main->color(_next_color);
        fill->color(_next_color);

        if (_next_brightness >= 50.0)
        {
          _next_color.rotateHue(_hue_step);
        }
      }

      void Leave::once()
      {
        unit<LedForest>("led forest")->leave();
        unit<ElWire>("el dance")->leave();
        unit<ElWire>("el entry")->leave();
        unit<DiscoBall>("discoball")->leave();

        main->black();
        fill->black();
      }

    } // namespace fx
  }   // namespace lightdesk
} // namespace pierre
