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
#include <vector>

namespace pierre {
namespace desk {

enum step_type_t : uint8_t { HueStep = 0, SatStep, BriStep, UnknownStepType };
inline step_type_t &operator++(step_type_t &stt) noexcept {
  stt = static_cast<step_type_t>(static_cast<uint8_t>(stt) + 1);
  return stt;
}

inline step_type_t operator++(step_type_t &stt, int) noexcept {
  return static_cast<step_type_t>(static_cast<uint8_t>(stt) + 1);
}

struct color_spec {
  friend fmt::formatter<color_spec>;

  enum id_t : uint8_t { A = 0, B, ABEnd };

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
    static constexpr auto kBOUND_PEAKS{"bound_peaks"sv};
    static constexpr auto kCOLORS{"colors"sv};
    static constexpr auto kNAME{"name"sv};
    static constexpr auto kSTEP_TYPE{"step_type"sv};
    static constexpr auto kSTEP{"step"sv};

    t.for_each([this](const toml::key &key, auto &&elem) {
      using T = std::decay_t<decltype(elem)>;

      // handle strings
      if constexpr (std::same_as<T, toml::value<string>>) {
        string val = elem.get();

        if (key == kNAME) {
          name = std::move(val);
        } else if (key == kSTEP_TYPE) {
          auto it = std::ranges::find(step_types, val);

          if (it < step_types.end()) {
            step_type = static_cast<step_type_t>(std::distance(step_types.begin(), it));
          } else {
            step_type = step_type_t::HueStep;
          }
        }
      } else if constexpr (std::same_as<T, toml::value<double>>) {

        if (key == kSTEP) step = elem.get();
      } else if constexpr (std::same_as<T, toml::array>) {

        if (key == kBOUND_PEAKS) {
          peaks.assign(elem);
        } else if (key == kCOLORS) {
          elem.for_each([this](const toml::table &t) { colors.emplace_back(t); });
        }
      }
    });
  }

  const auto &color(id_t id) const noexcept { return colors[id]; };

  static const auto id_type_str(id_t id) noexcept { return id_types[id]; }
  static const auto id_a() noexcept { return id_types[A]; }
  static const auto id_b() noexcept { return id_types[B]; }

  auto &operator[](id_t id) noexcept { return colors[id]; }

  const auto &step_type_str() const noexcept { return step_types[step_type]; }
  static const auto &step_type_str(step_type_t st) noexcept { return step_types[st]; }

private:
  // order dependent
  string name;
  double step;

  // order independent
  bound_peak peaks;
  std::vector<Hsb> colors;
  step_type_t step_type{UnknownStepType};

  static constexpr std::array id_types{"A", "B"};
  static constexpr std::array step_types{string("hue"), string("sat"), string("bri")};
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

    fmt::format_to(w, "name={} step_type={} step={} peaks={}", s.name, s.step_type_str(), s.step,
                   s.peaks);

    // write to ctx.out()
    return fmt::format_to(ctx.out(), "{}", msg);
  }
};