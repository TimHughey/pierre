/* Pierre - Custom Light Show via DMX for Wiss Landing
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

    https://www.wisslanding.com */

#pragma once

#include "base/helpers.hpp"
#include "base/minmax.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <ranges>
#include <typeinfo>

namespace pierre {

struct PeakMagBase {
  // Mag floor{36.4 * 1000};         // 36,400
  // Mag floor{3.65e4f};
  // Mag floor{3.7e4f};  // old pierre
  Magnitude floor{2.1};
  // Mag ceiling{2.1 * 1000 * 1000}; // 2.1 million
  // Mag ceiling{1.8e6f};
  // Mag ceiling{15.0f};  // old pierre
  Magnitude ceiling{28.0};
  // Magnitude strong{3.0}; // old pierre
  Magnitude strong{6.0};
};

struct PeakMagScaled {
  PeakMagBase base; // copy of the base data used to create this scale

  // Mag factor{2.41};
  Mag factor{2.3};
  Mag step{0.001};
  Mag floor{0.0};   // calculated by constructor
  Mag ceiling{0.0}; // calculated by constructor

  template <typename M = Mag> M interpolate(M m) const {
    return (scale_val<M>(m) - floor) / (ceiling - floor);
  }

  PeakMagScaled(const PeakMagBase &base) noexcept
      : base(base),                                 // the base scaled
        factor(1.0),                                //  factor(2.41) old pierre
        step(0.001),                                //
        floor(scale_val<Mag>(base.floor * factor)), //
        ceiling(scale_val<Mag>(base.ceiling))       //
  {}
};

} // namespace pierre