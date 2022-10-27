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
#include "frame/peak.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <type_traits>

namespace {
namespace ranges = std::ranges;
}

namespace pierre {

using peak_n_t = size_t; // represents peak of interest 1..max_peaks
using sentinel = std::default_sentinel_t;

class Peaks;
using peaks_t = std::shared_ptr<Peaks>;

class Peaks : public std::enable_shared_from_this<Peaks> {
public:
  static peaks_t create() { return std::shared_ptr<Peaks>(new Peaks()); }
  ~Peaks() = default;

private:
  Peaks() = default;

public:
  bool emplace(Magnitude m, Frequency f) noexcept;
  auto size() const noexcept { return std::ssize(peaks_map); } // define early for auto

  bool hasPeak(peak_n_t n) const {
    Magnitude low{0.0};
    // peak_n_t represents the peak of interest in the range of 1..max_peaks
    return peaks_map.size() > n ? true : false;
  }

  const Peak majorPeak() const noexcept {
    return !std::empty(peaks_map) ? std::begin(peaks_map)->second : Peak();
  }

  // find the first peak greater than the floating point value
  // specified in operator[]
  template <class T, class = typename std::enable_if<std::is_floating_point<T>::value>::type>
  const Peak operator[](T freq) {

    Peak found_peak;

    if (!std::empty(peaks_map)) {
      for (auto it = std::counted_iterator{std::begin(peaks_map), 5};
           (it != std::default_sentinel) && !found_peak; ++it) {
        const auto &[mag, peak] = *it;

        if (freq > peak.frequency()) found_peak = peak;
      }
    }

    return found_peak;
  }

  static bool silence(peaks_t peaks) {
    INFOX(module_id, "SILENCE", "use_count={} size={}\n", //
          peaks.use_count(), peaks.use_count() ? peaks->size() : false);

    return (peaks && !peaks->size()) == false;
  }

private:
  std::map<Magnitude, Peak, std::greater<Magnitude>> peaks_map;

public:
  static constexpr csv module_id{"PEAKS"};
};

} // namespace pierre
