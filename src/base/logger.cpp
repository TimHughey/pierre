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

#include "logger.hpp"
#include "elapsed.hpp"
#include "io.hpp"
#include "threads.hpp"
#include "types.hpp"

#include <algorithm>
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/compile.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <functional>
#include <iterator>
#include <latch>
#include <optional>
#include <ranges>
#include <string_view>

namespace ranges = std::ranges;
namespace views = std::ranges::views;

namespace pierre {

std::shared_ptr<Logger> Logger::self;
Elapsed Logger::elapsed_runtime; // class static data

const string Logger::format_chunk(ccs data, size_t bytes) const noexcept {
  constexpr std::string_view delim{"\n"};

  string chunk(data, bytes);
  string msg;
  auto w = std::back_inserter(msg);

  auto view = chunk                                     //
              | ranges::views::split(delim)             //
              | ranges::views::transform([](auto &&r) { //
                  return std::string_view(&*r.begin(), ranges::distance(r));
                });

  for (const auto line : view) {
    fmt::format_to(w, "{}{}\n", tab(), line);
  }

  return msg;
}

void Logger::print_chunk(const string &chunk) {
  constexpr std::string_view delim{"\n"};

  auto view = chunk                                     //
              | ranges::views::split(delim)             //
              | ranges::views::transform([](auto &&r) { //
                  return std::string_view(&*r.begin(), ranges::distance(r));
                });

  for (const auto line : view) {
    fmt::print("{}{}\n", tab(), line);
  }
}
Logger::millis_fp Logger::runtime() { // static
  return std::chrono::duration_cast<millis_fp>((Nanos)elapsed_runtime);
}

} // namespace pierre
