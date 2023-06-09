//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/dura_t.hpp"
#include "base/qpow10.hpp"
#include "base/types.hpp"

namespace pierre {

/// @brief Buffered audio stream details and frame timing
struct InputInfo {
  /// @brief audio data sample rate (in Hz)
  static constexpr uint32_t rate{44100}; // max available at the moment

  /// @brief number of audio channels
  static constexpr uint8_t channels{2};

  /// @brief bit depth of audio data
  static constexpr uint8_t bit_depth{16};

  /// @brief bytes per audio frane
  static constexpr uint8_t bytes_per_frame{4};

  /// @brief duration of a frame in nanoseconds
  static constexpr Nanos frame{static_cast<int64_t>(qpow10(9) / rate)};

  /// @brief Frames per specified duration
  /// @tparam T duration type
  /// @param v duration value
  /// @return number of frames
  template <typename T> static constexpr auto frame_count(T v) noexcept {
    if constexpr (std::same_as<T, Nanos>) {
      return v / lead_time;
    } else if constexpr (IsDuration<T>) {
      return std::chrono::duration_cast<Nanos>(v) / lead_time;
    } else {
      static_assert(AlwaysFalse<T>, "unhandled type");
    }
  }

  /// @brief Lead time for rendering a Frame in nanoseconds
  static constexpr Nanos lead_time{frame * 1024};

  /// @brief Lead time for rendering a Frame in raw microseconds
  static constexpr int64_t lead_time_us{std::chrono::duration_cast<Micros>(lead_time).count()};

  /// @brief Frames per second based on frame duration rounded to whole milliseconds
  ///        For use where fractional frames are not required
  static constexpr int fps{Millis(1000).count() /
                           std::chrono::duration_cast<Millis>(lead_time).count()};
};

} // namespace pierre
