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
    enum alt_t : uint8_t { Greater = 0, Freq, Mag, Last, EndOfAlts };
    std::array<bool, EndOfAlts> alts;

    static constexpr std::array<string, EndOfAlts> keys{"greater", "freq", "mag", "last"};

    bool greater{false};
    bool freq{false};
    bool mag{false};
    bool last{false};
    Hsb color;

    constexpr alternate() = default;
    alternate(alternate &&) = default;

    alternate(const toml::table &t) noexcept { assign(t); }

    void assign(const toml::table &t) noexcept {

      t.for_each(overloaded{[this](const toml::key &key, const toml::value<bool> &v) {
                              if (auto it = std::ranges::find(keys, key.str()); it != keys.end()) {
                                auto idx = std::distance(keys.begin(), it);
                                alts[idx] = *v;
                              }
                            },
                            [this](const toml::key &key, const toml::table &t) {
                              if (key == "color") color.assign(t);
                            }}

      );
    }

    bool &alt(alt_t idx) noexcept { return alts[idx]; }

    static const string &alt_desc(alt_t idx) noexcept { return keys[idx]; }
  };

  friend fmt::formatter<spot_spec::alternate>;

  struct fade_ctrl {
    Hsb color;
    Millis timeout;

    constexpr fade_ctrl() = default;
    fade_ctrl(fade_ctrl &&) = default;
    fade_ctrl &operator=(fade_ctrl &&) = default;

    void assign(const toml::table &t) noexcept {
      static constexpr auto kCOLOR{"color"sv};
      static constexpr auto kTIMEOUT{"timeout"sv};

      t.for_each([this](const toml::key &key, const toml::table &t) {
        if (key == kCOLOR) {
          color.assign(t);
        } else if (key == kTIMEOUT) {
          timeout = conf::dura::make(t);
        }
      });
    }
  };

  constexpr spot_spec() = default;

  /// @brief Create spot_spec configuration
  /// @param t Pointer to toml::table
  spot_spec(const toml::table *t) noexcept { assign(*t); }
  spot_spec(const toml::table &t) noexcept { assign(t); }

  spot_spec(const spot_spec &) = default;
  spot_spec(spot_spec &) = default;
  spot_spec(spot_spec &&) = default;

  spot_spec &operator=(const spot_spec &) = default;
  spot_spec &operator=(spot_spec &&) = default;

  void assign(const toml::table &t) noexcept {
    static constexpr auto kID{"id"};
    static constexpr auto kUNIT{"unit"};
    static constexpr auto kCOLOR_SPEC{"color_spec"};
    static constexpr auto kFADE{"fade"};
    static constexpr auto kALTERNATE{"alternate"};

    t.for_each([this](const toml::key &key, auto &&elem) {
      using T = std::decay_t<decltype(elem)>;

      if constexpr (std::same_as<T, toml::value<string>>) {
        if (key == kID) {
          id.assign(elem.get());
        } else if (key == kUNIT) {
          unit.assign(elem.get());
        } else if (key == kCOLOR_SPEC) {
          color_spec.assign(elem.get());
        }
      } else if constexpr (std::same_as<T, toml::table>) {
        if (key == kFADE) fade.assign(elem);
      } else if constexpr (std::same_as<T, toml::array>) {
        if (key == kALTERNATE) {
          elem.for_each([this](const toml::table &t) { alternates.emplace_back(t); });
        }
      }
    });
  }

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

namespace {
namespace desk = pierre::desk;
}

template <> struct fmt::formatter<desk::spot_spec> : public fmt::formatter<std::string> {

  template <typename FormatContext>
  auto format(const desk::spot_spec &s, FormatContext &ctx) const -> decltype(ctx.out()) {
    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "id={} unit='{}' color_spec={} fade[timeout={} {:h}]", s.id, s.unit,
                   s.color_spec, pierre::dura::humanize(s.fade.timeout), s.fade.color);

    for (const auto &alt : s.alternates) {
      fmt::format_to(w, " {}", alt);
    }

    return fmt::format_to(ctx.out(), "{}", msg);
  }
};

template <> struct fmt::formatter<desk::spot_spec::alternate> : public fmt::formatter<std::string> {

  template <typename FormatContext>
  auto format(const desk::spot_spec::alternate &alt, FormatContext &ctx) const
      -> decltype(ctx.out()) {

    using alt_t = pierre::desk::spot_spec::alternate::alt_t;

    std::string msg;
    auto w = std::back_inserter(msg);
    constexpr std::array alt_list{alt_t::Greater, alt_t::Freq, alt_t::Mag, alt_t::Last};

    std::ranges::for_each(alt_list, [&w, &alt](const auto &a) {
      fmt::format_to(w, "{}={} ", alt.alt_desc(a), alt.alts[a]);
    });

    fmt::format_to(w, "color={:h}", alt.color);

    return fmt::format_to(ctx.out(), "{}", msg);
  }
};