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

#include "base/io.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <stop_token>

namespace pierre {
namespace {
namespace ranges = std::ranges;
}

class Airplay;
using shAirplay = std::shared_ptr<Airplay>;

namespace shared {
extern std::shared_ptr<Airplay> airplay;
} // namespace shared

class Airplay : public std::enable_shared_from_this<Airplay> {
private:
  static constexpr int AIRPLAY_THREADS = 3;

private:
  Airplay() : watchdog_timer(io_ctx) {}

public:
  // shared instance management
  static shAirplay init() {
    shared::airplay = std::shared_ptr<Airplay>(new Airplay());
    return shared::airplay->init_self();
  }

  static shAirplay ptr() { return shared::airplay->shared_from_this(); }
  static void reset() { shared::airplay.reset(); }

  // void join() { thread_main.join(); }

  // void shutdown() {
  //   thread_main.request_stop();
  //   ranges::for_each(threads, [](Thread &t) { t.join(); });
  //   thread_main.join();
  // }

private:
  std::shared_ptr<Airplay> init_self();
  void watch_dog();

private:
  // order depdendent
  io_context io_ctx; // run by multiple threads
  steady_timer watchdog_timer;

  // order independent
  Threads threads;
  stop_tokens tokens;

public:
  static constexpr csv module_id{"AIRPLAY"};
};

} // namespace pierre
