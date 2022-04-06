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

#include "lightdesk/fx/fx.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class Leave : public Fx {

public:
  Leave(const float hue_step = 0.25f, const float brightness = 100.0f);
  ~Leave() = default;

  void executeFx(audio::spPeaks peaks) override;
  const string &name() const override {
    static const string fx_name = "Leave";

    return fx_name;
  }

  void once() override;

private:
  float _hue_step = 0.25f;
  float _brightness = 100.0f;

  float _next_brightness = 0;
  lightdesk::Color _next_color;

  spPinSpot main;
  spPinSpot fill;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre
