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

#include "base/bound.hpp"
#include "base/conf/token.hpp"
#include "base/conf/watch.hpp"
#include "base/dura.hpp"
#include "base/types.hpp"
#include "desk/color/color_spec.hpp"
#include "desk/color/hsb.hpp"
#include "desk/fx/majorpeak/spot_spec.hpp"

#include <array>
#include <assert.h>
#include <fmt/format.h>
#include <map>
#include <vector>

namespace pierre {

enum pinspot : uint8_t { Fill = 0, Main };

// place the code to load the configuration into the same namespace
// where other configuration functionality exists
namespace conf {

namespace {
namespace desk = pierre::desk;
}

struct majorpeak {
  friend fmt::formatter<majorpeak>;

  majorpeak(conf::token *tokc) noexcept : tokc{tokc} {}

  /// @brief Load the configuration for MajorPeak
  /// @return boolean indicating success or failure
  bool load() noexcept {
    // reset any warning or error messages produced by the previous load
    msgs.clear();

    auto *t = tokc->table();

    // ensure we have a configuration to work with
    if (t->empty()) {
      msgs.emplace_back("empty configuration");
      return false;
    }

    // we have configuration to work with, loop through it and load
    t->for_each([this](const toml::key &key, auto &&elem) {
      using T = std::decay_t<decltype(elem)>;

      if constexpr (std::same_as<T, toml::table>) {
        static constexpr auto kSILENCE{"silence"sv};
        static constexpr auto kCOLOR{"color"};

        if (key == kSILENCE) {
          silence_timeout = conf::dura::timeout_val(elem);
        } else if ((key == kCOLOR)) {
          if (auto node = elem[kCOLOR]; node.is_table()) {
            base_color.assign(*node.as_table());
          }
        }

      } else if constexpr (std::same_as<T, toml::array>) {
        static constexpr auto kCOLOR_SPEC{"color_spec"sv};
        static constexpr auto kSPOT_SPEC{"spot_spec"sv};

        if (key == kCOLOR_SPEC) {
          elem.for_each([this](const toml::table &t) { color_specs.emplace_back(t); });
        } else if (key == kSPOT_SPEC) {
          elem.for_each([this](const toml::table &t) { spot_specs.emplace_back(t); });
        }
      }
    });

    return msgs.empty();
  }

  // order dependent
  conf::token *tokc{nullptr};

  // warnings or errors resulting from loading the config
  std::vector<string> msgs;

  // order independent
  desk::Hsb base_color;
  Millis silence_timeout{20000};
  std::vector<desk::color_spec> color_specs;
  std::vector<desk::spot_spec> spot_specs;
};

} // namespace conf
} // namespace pierre

namespace {
namespace conf = pierre::conf;
}

template <> struct fmt::formatter<conf::majorpeak> : public fmt::formatter<std::string> {
  static constexpr auto ind = std::string_view{"\t\t\t\t\t\t "};

  template <typename FormatContext>
  auto format(const conf::majorpeak &mp, FormatContext &ctx) const -> decltype(ctx.out()) {
    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "silence_timeout={}\n", pierre::dura::humanize(mp.silence_timeout));

    for (const auto &color_spec : mp.color_specs) {
      fmt::format_to(w, "{}{}\n", ind, color_spec);
    }

    for (const auto &spot_spec : mp.spot_specs) {
      fmt::format_to(w, "{}{}\n", ind, spot_spec);
    }

    return fmt::format_to(ctx.out(), "{}", msg); // write to ctx.out()
  }
};