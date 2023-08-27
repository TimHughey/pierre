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

#include <algorithm>
#include <array>
#include <map>
#include <ranges>

namespace pierre {

class Peaks {
public:
  using all_peaks = std::vector<Peak>;

public:
  enum chan_t : size_t { Left = 0, Right };

public:
  Peaks() noexcept : chan_peaks{all_peaks(512), all_peaks(512)} {}
  ~Peaks() noexcept {}

  Peaks(Peaks &&) = default;
  Peaks &operator=(Peaks &&) = default;

private:
  constexpr bool empty(chan_t ch) const noexcept { return chan_peaks[ch].empty(); }
  constexpr const auto &peak1(chan_t ch) const noexcept { return chan_peaks[ch].front(); }
  auto size(chan_t ch = Left) const noexcept { return std::ssize(chan_peaks[ch]); }

public:
  /// @brief Are there audible peaks?
  /// @return bool3an
  bool audible() const noexcept { return !silence(); }

  void finalize() noexcept {
    for (auto &chp : chan_peaks) {
      std::ranges::sort(chp, std::ranges::greater(), &Peak::mag);
    }
  }

  // void insert(peak::Freq freq, peak::dB dB_norm, chan_t ch = Left) noexcept {
  // https://www.quora.com/What-is-the-maximum-allowed-audio-amplitude-on-the-standard-audio-CD

  // if (dB_norm > peak::dB(-76.0)) chan_peaks[ch].emplace_back(freq, dB_norm);
  //}

  void operator()(peak::Freq f, peak::Mag m, chan_t ch) noexcept {
    auto peak = Peak(f, m);

    if (peak.useable()) chan_peaks[ch].push_back(std::move(peak));
  }

  /// @brief Major Peak
  /// @param ch channel, Left (default), Right
  /// @return Reference to first (major) peak
  const Peak &operator()(chan_t ch = Left) const noexcept {
    return size(ch) ? peak1(ch) : silent_peak;
  }

  constexpr bool silence() const noexcept {

    return (std::empty(chan_peaks[Left]) && std::empty(chan_peaks[Right])) ||
           ((peak1(Left).mag <= peak::Mag()) && peak1(Right).mag <= peak::Mag());
  }

private:
  // order dependent
  bound_peak peak_bounds;
  Peak silent_peak;

  // order independent
  // std::array<peak_map_t, 2> maps;
  std::array<all_peaks, 2> chan_peaks;

public:
  MOD_ID("frame.peaks");
};

} // namespace pierre
