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

#include "base/helpers.hpp"
#include "base/minmax.hpp"
#include "base/typical.hpp"
#include "frame/peak_ref_data.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ranges>

namespace pierre {

struct Peak {
public:
  static PeakMagBase mag_base;     // see peaks_ref_data.hpp and peaks.cpp
  static PeakMagScaled mag_scaled; // see peaks_ref_data.hpp and peaks.cpp

public: // Peak
  Peak() noexcept {};
  Peak(const size_t i, const Freq f, const Mag m) noexcept : index(i), freq(f), mag(m) {}

  static MinMaxFloat magScaleRange() { return MinMaxFloat(0.0, mag_scaled.ceiling - mag_scaled.floor); }

  Freq frequency() const { return freq; }
  Freq frequencyScaled() const { return scale_val(freq); }
  bool greaterThanFloor() const { return mag_base.floor; }
  bool greaterThanFreq(Freq want_freq) const { return freq > want_freq; }

  Mag magnitude() const { return mag; }

  MagScaled magScaled() const {
    auto scaled = scale_val(mag) - mag_scaled.floor;

    return scaled > 0 ? scaled : 0;
  }

  bool magStrong() const { return mag >= (mag_base.floor * mag_base.strong); }

  explicit operator bool() const { return (mag > mag_base.floor) && (mag < mag_base.ceiling) ? true : false; }

  friend auto operator<=>(const auto &lhs, const auto &rhs) noexcept {
    if (lhs.mag < rhs.mag) return std::strong_ordering::less;
    if (lhs.mag > rhs.mag) return std::strong_ordering::greater;

    return std::strong_ordering::equal;
  }

  template <class T, class = typename std::enable_if<std::is_same<T, Mag>::value>::type> T scaled() const {
    auto x = scale_val(mag) - mag_scaled.floor;

    return x > 0 ? x : 0;
  }

  template <typename T> const T scaleMagToRange(const MinMaxPair<T> &range) const {
    auto ret_val = static_cast<T>(mag_scaled.interpolate(mag) * (range.max() - range.min()) + range.min());

    return ranges::clamp(ret_val, range.min(), range.max());
  }

  static const Peak zero() { return Peak(); }

private:
  size_t index = 0;
  Freq freq = 0;
  Mag mag = 0;
};

} // namespace pierre
