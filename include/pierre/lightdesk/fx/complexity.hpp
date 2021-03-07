/*
    lightdesk/fx/complexity.hpp -- LightDesk Effect With Complexity Threshold
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

#ifndef pierre_lightdesk_fx_complexity_hpp
#define pierre_lightdesk_fx_complexity_hpp

#include "lightdesk/fx/base.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class Complexity : public FxBase {
public:
  Complexity(const FxType type) : FxBase(type), _complexity_min(120.0) {}

  bool execute() override {
    if (checkComplexity()) {
      executeEffect();
    } else {
      completed();
    }

    return isComplete();
  }

protected:
  inline bool checkComplexity() const {
    // const float complexity = i2s()->complexityAvg();
    // if ((_complexity_min > 0.0) && (complexity < _complexity_min)) {
    //   return false;
    // }

    return true;
  }

  inline float &complexityMinimum() { return _complexity_min; }

private:
  float _complexity_min = 0.0;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
