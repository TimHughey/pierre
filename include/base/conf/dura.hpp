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

#include "base/conf/toml.hpp"
#include "base/dura_t.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <fmt/format.h>
#include <functional>
#include <ranges>

namespace pierre {
namespace conf {

struct dura {

  /// @brief Sum a toml::table into Millis
  /// @param t toml::table comprised of:
  ///          { min = 30, secs = 30, ms = 750}
  /// @return Millis
  static Millis make(const toml::table &t) noexcept {
    static constexpr const auto kTIMEOUT{"timeout"sv};

    if (t.contains(kTIMEOUT) && t[kTIMEOUT].is_table()) {
      // this is a top-level table that contains the timeout table
      // recurse into the timeout table
      return make(*t[kTIMEOUT].as_table());
    }

    // little lambda to check for the two different keys for
    // specifying durations
    constexpr auto extract = [](auto &&kl, auto &&k) -> bool {
      return std::ranges::find(kl, k) != kl.end();
    };

    Millis sum{0};

    // spin through the table and sum up the various durations
    t.for_each([&](const toml::key &key, const toml::value<int64_t> &elem) {
      if (extract(std::array{"minutes"sv, "min"sv}, key.str())) {
        sum += Minutes(elem.get());
      } else if (extract(std::array{"seconds"sv, "secs"sv}, key.str())) {
        sum += Seconds(elem.get());
      } else if (extract(std::array{"millis"sv, "ms"sv}, key.str())) {
        sum += Millis(elem.get());
      }
    });

    return sum;
  }

  static Millis timeout_val(toml::table *t) noexcept { return make(*t); }

  /// @brief Retrieve a "timeout" value from the config specified as:
  ///        silence = { timeout = {mins = 5, secs = 30, millis = 100 } }
  /// @param p path to the config value
  /// @param def_val default duration
  /// @return std::chrono::milliseconds

  static Millis timeout_val(const toml::table &base) noexcept { return make(base); }
};

} // namespace conf
} // namespace pierre
