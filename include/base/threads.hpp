
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

#include "base/types.hpp"

#include <algorithm>
#include <functional>
#include <pthread.h>
#include <ranges>
#include <set>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <vector>

namespace pierre {

using Threads = std::vector<Thread>;

void name_thread(csv name);
void name_thread(csv name, int num);

const string thread_id_short();

class stop_tokens {
public:
  stop_tokens() = default;

  void add(std::stop_token &&tok) { _tokens.emplace_back(tok); }

  bool any_requested() {
    return std::ranges::any_of(_tokens, [](auto &t) { return t.stop_requested(); });
  }

  template <typename S, typename F> bool any_requested(S &&stoppable, F &&func) {
    auto should_stop = std::ranges::any_of(_tokens, [](auto &t) { return t.stop_requested(); });

    if (should_stop) {
      stoppable.stop();
    } else {
      func();
    }

    return should_stop;
  }

  template <typename S, typename G, typename F>
  bool any_requested(S &&stoppable, G &&guard, F &&func) {
    auto should_stop = std::ranges::any_of(_tokens, [](auto &t) { return t.stop_requested(); });

    if (should_stop) {
      guard.reset();
      stoppable.stop();
    } else {
      func();
    }

    return should_stop;
  }

private:
  std::vector<std::stop_token> _tokens;
};

} // namespace pierre
