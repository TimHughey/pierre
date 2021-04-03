/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#ifndef pierre_core_hpp
#define pierre_core_hpp

#include <atomic>
#include <chrono>

namespace pierre {
namespace core {

typedef std::chrono::steady_clock clock;
typedef std::chrono::steady_clock::time_point time_point;
typedef std::chrono::milliseconds milliseconds;

class State {

public:
  ~State() = default;

  State(const State &) = delete;
  State &operator=(const State &) = delete;

  static void leave(milliseconds ms) {
    i.s.mode = Leaving;
    i.s.leaving.started = clock::now();
    i.s.leaving.ms = ms;
  }

  static bool leaveInProgress() {
    auto rc = false;

    auto elapsed = clock::now() - i.s.leaving.started;

    if (elapsed < i.s.leaving.ms) {
      rc = true;
    }

    return rc;
  }

  static bool leaving() { return i.s.mode == Leaving; }

  template <typename T> static T leavingDuration() {
    return std::chrono::duration_cast<T>(i.s.leaving.ms);
  }
  static void quit() { i.s.mode = Quitting; }
  static bool quitting() { return i.s.mode == Quitting; }
  static bool running() { return i.s.mode != Shutdown; }

  static void shutdown() { i.s.mode = Shutdown; }

private:
  State() = default;

private:
  typedef enum { Running = 0, Leaving, Shutdown, Quitting } Mode;

private:
  static State i;

  struct {
    std::atomic<Mode> mode = Running;

    struct {
      time_point started;
      milliseconds ms;
    } leaving;
  } s;
};

} // namespace core
} // namespace pierre

#endif
