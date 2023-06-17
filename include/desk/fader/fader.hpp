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
    int64_t to_travel{0};
    int64_t traveled{0};
    bool finished{true};

    constexpr bool active() const { return !finished; }
    constexpr bool complete() const { return traveled > to_travel; }
    constexpr void finish() noexcept { finished = true; }
    constexpr void initiate() noexcept { finished = false; }
    constexpr double progress() const { return (double)traveled / (double)to_travel; }

    constexpr travel_frames &reset() noexcept {
      *this = travel_frames{};
      return *this;
    }

    template <typename _T>
      requires std::convertible_to<_T, int64_t>
    constexpr auto &operator-=(const _T &rhs) noexcept {
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

    void assign_now(const Hsb &next_now) noexcept { now = next_now; }

    Hsb dest{};
    Hsb now{};
    Hsb origin{};
  };

public:
  Fader() = default;
  virtual ~Fader(){};
  Fader(Nanos d) noexcept { assign(d); }
  Fader(Nanos d, travel_colors::origin_dest &&od) noexcept {
    assign(std::move(d), std::forward<travel_colors::origin_dest>(od)); // setup fader colors
  }

  bool active() const noexcept { return frames.active(); }

  void assign(Nanos d) noexcept { frames.to_travel = InputInfo::frame_count(d); }

  void assign(Nanos d, travel_colors::origin_dest &&od) noexcept {
    assign(std::move(d));
    colors.assign(std::forward<travel_colors::origin_dest>(od));
  }

  constexpr bool complete() const noexcept { return frames.complete(); }

  virtual Hsb &color_now() noexcept { return colors.now; }

  void initiate(Nanos d, travel_colors::origin_dest &&od) noexcept {
    frames.reset();
    assign(d);
    initiate(std::forward<travel_colors::origin_dest>(od));
  }

  void initiate(travel_colors::origin_dest &&od) noexcept {

    colors.assign(std::forward<travel_colors::origin_dest>(od));

    if (frames.to_travel > 0) frames.initiate();
  }

  virtual bool travel() noexcept {

    if (frames.active()) {
      travel_hook();

      ++frames;

      if (frames.complete()) {
        finish_hook();
        frames.finish();
      }
    }

    return frames.active();
  }

protected:
  /// @brief Hook for subclasses participate in fader finish
  virtual void finish_hook() noexcept {}

  /// @brief Hook for subclasses to participate in traveling
  ///        The default implementation when Fader is not subclassed
  ///        is to fade from origin to destination via color brightness
  /// @return Percentage complete
  virtual double travel_hook() noexcept {
    // if we've arrived at the destination signal to stop traveling
    if (colors.origin.bri == colors.dest.bri) return 0.0;

    auto fade_level = frames.progress();

    if (colors.dest.bri == 0.0_BRI) {
      colors.now = colors.origin;
      colors.now.bri -= colors.origin.bri * Bri(fade_level);
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

public:
  MOD_ID("desk.fader");
};

} // namespace desk
} // namespace pierre
