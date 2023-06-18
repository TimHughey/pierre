//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2023  Tim Hughey
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

#include "base/bound.hpp"
#include "base/types.hpp"
#include "desk/color/hsb.hpp"
#include "frame/peaks/bound_peak.hpp"
#include "frame/peaks/peak.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fmt/format.h>
#include <iterator>
#include <ranges>
#include <tuple>
#include <vector>

namespace pierre {
namespace desk {

enum step_types : uint8_t { HueStep = 0, SatStep, BriStep, UnknownStepType };

struct color_spec {
  friend fmt::formatter<color_spec>;

  color_spec() = default;

  color_spec(const toml::table *t) noexcept { assign(*t); }

  /// @brief Create color_spec configuration
  /// @param t Pointer to toml::table shaped as:
  ///
  ///          {
  ///            name = "generic"
  ///            bound_peaks = [
  ///              { freq = 40.0, mag = 2.19 },
  ///              { freq = 1100.0, mag = 85.0 }
  ///            ]
  ///            A = { hue = 0.0, sat = 100.0, bri = 0.0 }
  ///            B = {hue = 340.0, sat = 100.0, bri = 100.0}
  ///            step_type = "hue"
  ///            step = 0.1
  ///          }
  color_spec(const toml::table &t) noexcept { assign(t); }

  color_spec(const color_spec &) = default;
  color_spec(color_spec &) = default;
  color_spec(color_spec &&) = default;

  color_spec &operator=(const color_spec &) = default;
  color_spec &operator=(color_spec &&) = default;

  void assign(const toml::table &t) noexcept {

    t.for_each(overloaded{
        [this](const toml::key &key, const toml::value<string> &s) {
          if (key == "name") {
            name = *s;
          } else if (key == "step_type") {
            if (auto it = std::ranges::find(step_types_str, *s); it < step_types_str.end()) {
              step_type = static_cast<step_types>(std::distance(step_types_str.begin(), it));
            } else {
              step_type = step_types::HueStep;
            }
          }
        },
        [this](const toml::key &key, const toml::value<double> v) {
          if (key == "step") step = *v;
        },
        [this](const toml::key &key, const toml::array &arr) {
          if (key == "bound_peaks") {
            peaks.assign(arr);
          } else if (key == "colors") {
            arr.for_each([this](const toml::table &t) { colors.emplace_back(t); });
          }
        }});
  }

  template <typename T>
    requires IsSpecializedColorPart<T>
  auto color_range() const noexcept {
    return std::pair{static_cast<T>(colors[0]), static_cast<T>(colors[1])};
  }

  constexpr const auto &spec_name() const { return name; }

  auto &operator[](auto idx) noexcept { return colors[idx]; }

  auto operator==(const string &n) const { return n == name; }

  auto match_peak(const Peak &p) const noexcept { return p.inclusive<peak::Freq>(peaks); }

  const auto &step_type_str() const noexcept { return step_types_str[step_type]; }
  static const auto &step_type_str(step_types st) noexcept { return step_types_str[st]; }

public:
  // order dependent
  string name;
  double step;

  // order independent
  bound_peak peaks;
  std::vector<Hsb> colors;
  step_types step_type{UnknownStepType};

  // static constexpr std::array id_types{"A", "B"};
  static constexpr std::array step_types_str{string("hue"), string("sat"), string("bri")};
};
} // namespace desk
} // namespace pierre

namespace {
namespace desk = pierre::desk;
}

template <> struct fmt::formatter<desk::color_spec> : public fmt::formatter<std::string> {

  template <typename FormatContext>
  auto format(const desk::color_spec &s, FormatContext &ctx) const -> decltype(ctx.out()) {
    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{:<8} {:<3} step={} peaks={}", s.name, s.step_type_str(), s.step, s.peaks);

    // write to ctx.out()
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};