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
#include "base/typical.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <stop_token>

namespace pierre {

class Airplay;
typedef std::shared_ptr<Airplay> shAirplay;

namespace shared {
extern std::optional<shAirplay> __airplay;
std::optional<shAirplay> &airplay() { return shared::__airplay; };
} // namespace shared

class Airplay : public std::enable_shared_from_this<Airplay> {
private:
  static constexpr int AIRPLAY_THREADS = 3;

private:
  Airplay() : watchdog_timer(io_ctx) {}

public:
  // shared instance management
  static shAirplay init(); // create and start threads, dependencies
  static shAirplay ptr() { return shared::airplay().value()->shared_from_this(); }
  static void reset() { shared::airplay().reset(); }

  void join() { thread_main.join(); }

  void shutdown() {
    thread_main.request_stop();
    ranges::for_each(threads, [](Thread &t) { t.join(); });
    thread_main.join();
  }

  // misc debug
  static constexpr csv moduleID() { return module_id; }

private:
  void watch_dog();

private:
  // order depdendent
  io_context io_ctx; // run by multiple threads
  steady_timer watchdog_timer;

  // order independent
  Thread thread_main;
  Threads threads;
  std::stop_token stop_token;

  static constexpr csv module_id = "AIRPLAY";
};

} // namespace pierre
