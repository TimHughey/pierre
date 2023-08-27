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
#include "base/conf/dura.hpp"
#include "base/dura.hpp"
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

struct spot_spec {
  friend fmt::formatter<spot_spec>;

  struct alternate {
    enum alt_t : uint8_t { Greater = 0, Freq, Spl, Last, EndOfAlts };
    std::array<bool, EndOfAlts> alts;

    static constexpr std::array<string, EndOfAlts> keys{"greater", "freq", "spl", "last"};

    Hsb color;

    constexpr alternate() = default;
    alternate(alternate &&) = default;

    alternate(const toml::table &t) noexcept { assign(t); }

    void assign(const toml::table &t) noexcept {

      t.for_each([this](const toml::key &key, auto &&elem) {
        elem.visit([&, this](const toml::value<bool> &v) {
          if (auto it = std::ranges::find(keys, key.str()); it != keys.end()) {
            alts[std::distance(keys.begin(), it)] = *v;
          }
        });

        elem.visit([&, this](const toml::table &t) {
          if (key == "color") color.assign(t);
        });
      });
    }

    bool &alt(alt_t idx) noexcept { return alts[idx]; }

    static const string &alt_desc(alt_t idx) noexcept { return keys[idx]; }

    string format() const noexcept;
  };

  friend fmt::formatter<spot_spec::alternate>;

  struct fade_ctrl {
    Hsb color;
    Millis timeout;

    constexpr fade_ctrl() = default;
    fade_ctrl(fade_ctrl &&) = default;
    fade_ctrl &operator=(fade_ctrl &&) = default;

    void assign(const toml::table &t) noexcept {

      t.for_each([this](const toml::key &key, const toml::table &t) {
        if (key == "color") {
          color.assign(t);
        } else if (key == "timeout") {
          timeout = conf::dura::make(t);
        }
      });
    }
  };

  constexpr spot_spec() = default;

  /// @brief Create spot_spec configuration
  /// @param t Reference to toml::table
  spot_spec(const toml::table &t) noexcept { assign(t); }

  spot_spec(const spot_spec &) = default;
  spot_spec(spot_spec &) = default;
  spot_spec(spot_spec &&) = default;

  spot_spec &operator=(const spot_spec &) = default;
  spot_spec &operator=(spot_spec &&) = default;

  void assign(const toml::table &t) noexcept {

    t.for_each([this](const toml::key &key, auto &&elem) {
      elem.visit([&, this](const toml::value<string> &s) {
        if (key == "id") id.assign(*s);
        if (key == "unit") unit.assign(*s);
        if (key == "color_spec") color_spec.assign(*s);
      });

      elem.visit([&, this](const toml::table &t) {
        if (key == "fade") fade.assign(t);
      });

      elem.visit([&, this](const toml::array &arr) {
        arr.for_each([this](const toml::table &t) { alternates.emplace_back(t); });
      });
    });
  }

  string format() const noexcept;

  const string &operator()() const { return id; }

  // order dependent
  string id;
  string unit;
  string color_spec;

  fade_ctrl fade;
  std::vector<alternate> alternates;
};

} // namespace desk
} // namespace pierre

template <> struct fmt::formatter<desk::spot_spec> : public fmt::formatter<std::string> {
  using spot_spec = pierre::desk::spot_spec;

  template <typename FormatContext>
  auto format(const spot_spec &s, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", s.format());
  }
};

template <> struct fmt::formatter<desk::spot_spec::alternate> : public fmt::formatter<std::string> {
  using alternate = pierre::desk::spot_spec::alternate;

  template <typename FormatContext>
  auto format(const alternate &alt, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", alt.format());
  }
};