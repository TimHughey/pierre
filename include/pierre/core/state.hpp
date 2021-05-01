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

#ifndef pierre_core_state_hpp
#define pierre_core_state_hpp

#include <atomic>
#include <chrono>
#include <memory>
// #include <string>

#include "core/config.hpp"

using namespace std::string_view_literals;

namespace pierre {
namespace core {

typedef std::chrono::steady_clock clock;
typedef std::chrono::steady_clock::time_point time_point;
typedef std::chrono::milliseconds milliseconds;

class State {
public:
  typedef std::string_view string_view;

public:
  ~State() = default;

  State(const State &) = delete;
  State &operator=(const State &) = delete;

  static std::shared_ptr<Config> config();
  static toml::table *config(const string_view &key);
  static toml::table *config(const string_view &key, const string_view &sub);

  static std::shared_ptr<Config> initConfig() noexcept;

  static bool isRunning();
  static bool isSilent();
  static bool isSuspended();

  static void leave(milliseconds ms);
  static bool leaveInProgress();
  static bool leaving();

  template <typename T> static T leavingDuration() {
    return std::chrono::duration_cast<T>(i.s.leaving.ms);
  }

  static void quit();
  static bool quitting();

  static void silent(bool silent);

  static void shutdown();

private:
  State() = default;

private:
  typedef enum {
    Running = 0,
    Leaving,
    Shutdown,
    Silence,
    Suspend,
    Quitting
  } Mode;

private:
  static State i;

  std::shared_ptr<Config> _cfg;

  struct {
    std::atomic<Mode> mode = Suspend;

    struct {
      time_point started;
      milliseconds ms;
    } leaving;

    struct {
      bool detected = false;
      time_point started;
      Mode prev_mode;
    } silence;
  } s;
};

} // namespace core
} // namespace pierre

#endif
