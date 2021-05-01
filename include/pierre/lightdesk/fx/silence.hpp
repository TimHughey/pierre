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
#ifndef pierre_lightdesk_fx_silence_hpp
#define pierre_lightdesk_fx_silence_hpp

#include "lightdesk/fx/fx.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class Silence : public Fx {

public:
  Silence() = default;
  ~Silence() = default;

  void executeFx(audio::spPeaks peaks) override {
    peaks.reset();
    // do nothing, enjoy the silence
  };

  const string &name() const override {
    static const string fx_name = "Silence";

    return fx_name;
  }

  void once() override { allUnitsDark(); }
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
