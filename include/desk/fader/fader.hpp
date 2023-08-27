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
#include "base/input_info.hpp"
// #include "base/logger.hpp"
#include "base/types.hpp"
#include "desk/color/hsb.hpp"

#include <array>
#include <ranges>

namespace pierre {

namespace desk {

template <typename T, typename U>
concept FaderCapable = requires(T v) {
  v.finish();

  // ensure we have a travel function
  requires requires(typename T::value_type v, double total, double current) {
    { v.travel(total, current) } -> std::same_as<double>;
  };
};

class Fader {
public:
  struct travel_frames {
    constexpr travel_frames() = default;
    constexpr travel_frames(Nanos d) noexcept { to_travel = InputInfo::frame_count(d); }

    int64_t to_travel{0};
    int64_t traveled{0};

    /// @brief Travel complete after at least one frame and
    ///        frames traveled exceeds frames to travel
    /// @return Boolean
    constexpr bool complete() const { return (traveled > 0) && (traveled > to_travel); }
    constexpr double progress() const {
      return std::clamp((double)traveled / (double)to_travel, 0.0, 1.0);
    }

    constexpr travel_frames &reset() noexcept {
      *this = travel_frames{};
      return *this;
    }

    template <typename _T>
      requires std::convertible_to<_T, int64_t>
    constexpr travel_frames &operator-=(const _T &rhs) noexcept {
      traveled -= rhs;
      return *this;
    }

    template <typename _T>
      requires std::convertible_to<_T, int64_t>
    constexpr auto &operator+=(const _T &rhs) noexcept {
      traveled += rhs;
      return *this;
    }

    constexpr auto &operator++() noexcept {
      *this += 1;
      return *this;
    }

    constexpr auto &operator--() noexcept {
      *this -= 1;
      return *this;
    }
  };

  struct travel_colors {
    using origin_dest = std::array<Hsb, 2>;

    void assign(origin_dest &&vals) noexcept {
      origin = vals[0]; // array[0] is origin
      dest = vals[1];   // array[1] is dest
    }

    Hsb dest{};
    Hsb now{};
    Hsb origin{};
  };

public:
  Fader() = default;
  virtual ~Fader() {}

  bool active() const noexcept { return !finished && fading; }

  void assign(Nanos d, travel_colors::origin_dest &&od) noexcept {
    frames = travel_frames(d);
    colors.assign(std::forward<travel_colors::origin_dest>(od));
  }

  constexpr bool complete() const noexcept { return finished; }

  virtual Hsb &color_now() noexcept { return colors.now; }

  void initiate(Nanos d, travel_colors::origin_dest &&od) noexcept {
    *this = Fader();

    assign(d, std::forward<travel_colors::origin_dest>(od));
    fading = true;
  }

  /// @brief Progresses the fader
  /// @return Boolean, true=continue traveling, false=destination reached
  virtual bool travel() noexcept {

    if (final_frame) {
      finish_hook();
      finished = true;
      fading = false;
    } else if (!finished) {
      travel_hook();

      ++frames;

      if (frames.complete()) final_frame = true;
    }

    return finished;
  }

protected:
  /// @brief Hook for subclasses participate in fader finish
  virtual void finish_hook() noexcept { colors.now = colors.dest; }

  /// @brief Hook for subclasses to participate in traveling
  ///        The default implementation when Fader is not subclassed
  ///        is to fade from origin to destination via color brightness
  /// @return Percentage complete
  virtual double travel_hook() noexcept {
    auto fade_level = std::clamp(frames.progress(), 0.0, 1.0);

    if (colors.dest.bri == 0.0_BRI) {
      colors.now = colors.origin;
      colors.now.bri = colors.origin.bri - Bri(colors.origin.bri.get() * fade_level);
    } else {
      colors.now = colors.dest;
      colors.now.bri *= Bri(fade_level);
    }

    return static_cast<double>(fade_level);
  }

protected:
  // order independent
  travel_frames frames;
  travel_colors colors;

  bool fading{false};
  bool finished{false};
  bool final_frame{false};

public:
  MOD_ID("desk.fader");
};

} // namespace desk
} // namespace pierre
