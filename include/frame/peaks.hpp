// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/logger.hpp"
#include "base/types.hpp"
#include "frame/peaks/peak.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <type_traits>

namespace pierre {

class Peaks {
public:
  enum CHANNEL : size_t { LEFT = 0, RIGHT };

private:
  // sorted descending
  using peak_map_t = std::map<Magnitude, Peak, std::greater<Magnitude>>;

public:
  Peaks() = default;

public:
  bool emplace(Magnitude m, Frequency f, CHANNEL channel = LEFT) noexcept;
  size_t size(CHANNEL channel = LEFT) const noexcept { return std::ssize(peaks_map[channel]); }

  bool has_peak(size_t n, CHANNEL channel = LEFT) const noexcept {
    return peaks_map[channel].size() > n ? true : false;
  }

  const Peak major_peak(CHANNEL channel = LEFT) const noexcept {
    const auto &map = peaks_map[channel];

    return !std::empty(map) ? std::begin(map)->second : Peak();
  }

  // find the first peak greater than the Frequency
  const Peak operator()(const Frequency freq, CHANNEL channel = LEFT) const noexcept {
    Peak found;
    const auto &map = peaks_map[channel];

    if (!std::empty(map)) {
      for (auto it = std::counted_iterator{std::begin(map), 5};
           (it != std::default_sentinel) && !found; ++it) {
        const auto &[mag, peak] = *it;

        if (freq > peak.frequency()) found = peak;
      }
    }

    return found;
  }

  bool silence() const noexcept { return peaks_map[LEFT].empty() && peaks_map[RIGHT].empty(); }

private:
  std::array<peak_map_t, 2> peaks_map{peak_map_t(), peak_map_t()};

public:
  static constexpr csv module_id{"PEAKS"};
};

} // namespace pierre
