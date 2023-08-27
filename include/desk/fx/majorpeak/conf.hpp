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
#include "frame/peaks/bound_spl.hpp"

#include <algorithm>
#include <array>
#include <assert.h>
#include <fmt/format.h>
#include <map>
#include <ranges>
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

  constexpr majorpeak(conf::token *tokc) noexcept : tokc{tokc} {}

  constexpr bool empty() const noexcept { return color_specs.empty() || spot_specs.empty(); }
  string format() const noexcept;

  /// @brief Load the configuration for MajorPeak
  /// @return boolean indicating success or failure
  bool load() noexcept {
    // reset any warning or error messages produced by the previous load
    msgs.clear();

    auto &t = tokc->table();

    // ensure we have a configuration to work with
    if (t.empty()) {
      msgs.emplace_back("empty configuration");
      return false;
    }

    // we have a configuration table to work with that contains tables and arrays
    // loop through the key/val pairs and handle each key
    t.for_each(overloaded{
        [this](const toml::key &key, const toml::table &subt) {
          if (key == "silence") {
            silence_timeout = conf::dura::timeout_val(subt);
          } else if (key == "color") {
            base_color.assign(subt);
          }
        },
        [this](const toml::key &key, const toml::array &arr) {
          if (key == "color_spec") {
            arr.for_each([this](const toml::table &subt) { color_specs.emplace_back(subt); });
          } else if (key == "spot_spec") {
            arr.for_each([this](const toml::table &subt) { spot_specs.emplace_back(subt); });
          } else if (key == "spl_range") {
            spl_bound.assign(arr);
          }
        }});

    return msgs.empty();
  }

  // order dependent
  conf::token *tokc{nullptr};

  // warnings or errors resulting from loading the config
  std::vector<string> msgs;

  // order independent
  desk::Hsb base_color;
  Millis silence_timeout{20000};
  bound_spl spl_bound;
  std::vector<desk::color_spec> color_specs;
  std::vector<desk::spot_spec> spot_specs;
};

} // namespace conf
} // namespace pierre

namespace {
namespace conf = pierre::conf;
}

template <> struct fmt::formatter<conf::majorpeak> : public fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const conf::majorpeak &mp, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", mp.format()); // write to ctx.out()
  }
};