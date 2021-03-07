/*
    lightdesk/fx/factory.hpp -- LightDesk Effect Factory
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

#ifndef pierre_lightdesk_fx_factory_hpp
#define pierre_lightdesk_fx_factory_hpp

#include "lightdesk/fx/all.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

typedef class FxFactory FxFactory_t;

class FxFactory {
public:
  inline static FxBase_t *create(FxType_t type) {
    FxBase_t *fx = nullptr;

    switch (type) {
    case fxSimpleStrobe:
      fx = new SimpleStrobe();
      break;

    case fxFastStrobeSound:
      fx = new SoundFastStrobe();
      break;

    case fxWashedSound:
      fx = new WashedSound();
      break;

    case fxMajorPeak:
      fx = new MajorPeak();
      break;

    default:
      fx = new Basic(type);
      break;
    }

    return fx;
  }
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
