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

#include "base/minmax.hpp"
#include "base/typical.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ranges>

namespace pierre {

template <typename T> constexpr T scale_val(T val) { //
  return (val <= 0.0) ? 0.0 : std::log10(val);
}

struct Peak {
private:
  struct mag_base {
    static constexpr Mag floor = 36.4 * 1000;         // 36,400
    static constexpr Mag ceiling = 2.1 * 1000 * 1000; // 2.1 million
    static constexpr Mag strong = 3.0;
  };

  struct mag_scaled {
    static constexpr Mag factor = 2.41;
    static constexpr Mag step = 0.001;
    static constexpr Mag floor = scale_val(mag_base::floor * factor);
    static constexpr Mag ceiling = scale_val(mag_base::ceiling);

    static constexpr Mag interpolate(Mag m) {
      return (scale_val(m) - mag_scaled::floor) / (mag_scaled::ceiling - mag_scaled::floor);
    }
  };

public: // Peak
  Peak() = default;
  Peak(const size_t i, const Freq f, const Mag m) : index(i), freq(f), mag(m) {}

  static MinMaxFloat magScaleRange() {
    return MinMaxFloat(0.0, mag_scaled::ceiling - mag_scaled::floor);
  }

  Freq frequency() const { return freq; }
  Freq frequencyScaled() const { return scale_val(freq); }
  bool greaterThanFloor() const { return mag > Peak::magFloor(); }
  bool greaterThanFreq(Freq want_freq) const { return freq > want_freq; }

  Mag magnitude() const { return mag; }
  static constexpr Mag magFloor() { return mag_base::floor; }

  MagScaled magScaled() const {
    auto scaled = scale_val(mag) - mag_scaled::floor;

    return scaled > 0 ? scaled : 0;
  }

  bool magStrong() const { return mag >= (mag_base::floor * mag_base::strong); }

  explicit operator bool() const {
    return (mag > mag_base::floor) && (mag < mag_base::ceiling) ? true : false;
  }

  template <typename T> const T scaleMagToRange(const MinMaxPair<T> &range) const {
    auto ret_val = static_cast<T>(                                               //
        mag_scaled::interpolate(mag) * (range.max() - range.min()) + range.min() //
    );

    return ranges::clamp(ret_val, range.min(), range.max());
  }

  static constexpr Peak zero() { return Peak(); }

private:
  size_t index = 0;
  Freq freq = 0;
  Mag mag = 0;
};

} // namespace pierre
