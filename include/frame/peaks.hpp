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

#include "base/types.hpp"
#include "frame/peaks/bound_peak.hpp"
#include "frame/peaks/peak.hpp"

#include <array>
#include <map>
#include <ranges>

namespace pierre {

class Peaks {
public:
  // using freq_min_max = min_max_pair<Frequency>;
  // using mag_min_max = min_max_pair<Magnitude>;
  using peak_map_t = std::map<Magnitude, Peak, std::greater<Magnitude>>; // descending

public:
  enum chan_t : size_t { Left = 0, Right };

public:
  Peaks() noexcept : peak_bounds(Peak(Magnitude(2.17)), Peak(Magnitude(85.0))) {}
  ~Peaks() noexcept {}

  Peaks(Peaks &&) = default;
  Peaks &operator=(Peaks &&) = default;

private:
  bool empty(chan_t ch) const noexcept { return maps[ch].empty(); }
  const auto &peak1(chan_t ch) const noexcept { return maps[ch].begin()->second; }
  ssize_t size(chan_t channel = Left) const noexcept { return std::ssize(maps[channel]); }

public:
  /// @brief Are there audible peaks?
  /// @return bool3an
  bool audible() const noexcept { return !silence(); }

  bool insert(Magnitude m, Frequency f, chan_t channel = Left) noexcept {
    if (peak_bounds.inclusive(m)) {
      auto [_, inserted] = maps[channel].try_emplace(m, f, m);
      return inserted;
    }

    return false;
  }

  /// @brief Discover the first element that is not less than peak using the magnitude
  /// @param peak Peak to use for discovery
  /// @param channel Channel (defaults to Left)
  /// @return const reference to the Peak found or silent peak
  const auto lower_bound(const Peak &peak, chan_t channel = Left) const noexcept {

    auto it = maps[channel].lower_bound(peak.val<Magnitude>());

    return (it != maps[channel].end()) ? it->second : silent_peak;
  }

  /// @brief Major Peak
  /// @param ch channel, Left (default), Right
  /// @return Reference to first (major) peak
  const Peak &operator()(chan_t ch = Left) const noexcept {
    return size(Left) ? peak1(ch) : silent_peak;
  }

  // find the first peak greater than the Frequency
  const Peak &operator()(const Frequency &freq, chan_t channel = Left) const noexcept {

    if (size(Left)) {
      const auto &map = maps[channel];

      for (auto &[mag, peak] : std::views::counted(map.begin(), 5)) {
        if (freq > peak.freq) return peak;
      }
    }

    return silent_peak;
  }

  bool silence() const noexcept { return empty(Left) || empty(Right); }

private:
  // order dependent
  bound_peak peak_bounds;
  Peak silent_peak;

  // order independent
  std::array<peak_map_t, 2> maps;

public:
  MOD_ID("frame.peaks");
};

} // namespace pierre
