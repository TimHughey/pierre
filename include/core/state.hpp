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

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

#include "args.hpp"
#include "config.hpp"

using namespace std::string_view_literals;
using namespace std::chrono;

namespace pierre {

class State {
  using string = std::string;

public:
  ~State() = default;

  State(const State &) = delete;
  State &operator=(const State &) = delete;

  static ConfigPtr config();
  static bool initConfig(const string cfg_file) noexcept;

  static bool isRunning();
  static bool isSilent();
  static bool isSuspended();

  static void leave(milliseconds ms);
  static bool leaveInProgress();
  static bool leaving();

  template <typename T> static T leavingDuration() {
    return duration_cast<T>(i.s.leaving.ms);
  }

  static void quit();
  static bool quitting();

  static void setup(std::unique_ptr<Args> args);

  static void silent(bool silent);

  static void shutdown();

private:
  State() = default;

public:
  enum class Mode {
    Running = 0,
    Leaving,
    Shutdown,
    Silence,
    Suspend,
    Quitting
  };

private:
  static State i;

  ConfigPtr cfg;

  struct {
    using enum Mode;
    std::atomic<Mode> mode = Suspend;

    struct {
      steady_clock::time_point started;
      milliseconds ms;
    } leaving;

    struct {
      bool detected = false;
      std::chrono::steady_clock::time_point started;
      Mode prev_mode;
    } silence;
  } s;
};

} // namespace pierre
