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

#include "iostream"

#include "args.hpp"
#include "state.hpp"

using namespace std::chrono;
using string = std::string;
using string_view = std::string_view;

namespace pierre {

using namespace std;
using std::chrono::duration_cast;

State State::i = State();

ConfigPtr State::config() { return i.cfg; }

bool State::initConfig(const string cfg_file) noexcept {
  i.cfg = make_shared<Config>();

  return true;
}

bool State::isSilent() { return i.s.mode == Mode::Silence; }
bool State::isSuspended() { return i.s.mode == Mode::Suspend; }
bool State::isRunning() { return i.s.mode != Mode::Shutdown; }

void State::setup(std::unique_ptr<Args> args) {}

void State::silent(bool silent) {
  auto &silence = i.s.silence;

  // if this is the first detection of silence start tracking the duration
  if (silent && !silence.detected) {
    silence.detected = true;
    silence.started = steady_clock::now();
    return;
  }

  // once we're suspended there is nothing more to do with
  // additional silence
  if (silent && isSuspended()) {
    return;
  }

  // this is more silence, do we need to transition to Silence or Suspend?
  // if (silent && silence.detected) {
  //   auto diff = duration_cast<milliseconds>(clock::now() - silence.started);
  //   auto tbl = State::config()->table();

  //   // once we're in Silent mode check if we shuld progress to Suspend
  //   if (isSilent() && !isSuspended()) {
  //     auto ms = tbl["silence"sv]["suspend_after_ms"sv].value_or(600000);

  //     if (diff.count() > ms) {
  //       i.s.mode = Mode::Suspend; // engage Suspend mode
  //     }
  //   }

  //   // if we're still in Running mode then track how long it's been
  //   // silent and eventually progress to Silence mode

  //   if (i.s.mode == Mode::Running) {
  //     auto min_ms = tbl["silence"sv]["min_ms"sv].value_or(13000);

  //     if (diff.count() > min_ms) {
  //       silence.prev_mode = i.s.mode;
  //       i.s.mode = Mode::Silence; // engage Silence mode
  //     }
  //   }
  // }

  if (silent == false) {
    silence.detected = false;
    if ((i.s.mode == Mode::Silence) || (i.s.mode == Mode::Suspend)) {
      // return to the active mode prior to Silence/Suspend
      i.s.mode = silence.prev_mode;
    }
  }
}

void State::leave(milliseconds ms) {
  i.s.mode = Mode::Leaving;
  i.s.leaving.started = steady_clock::now();
  i.s.leaving.ms = ms;
}

bool State::leaveInProgress() {
  auto rc = false;

  auto elapsed = steady_clock::now() - i.s.leaving.started;

  if (elapsed < i.s.leaving.ms) {
    rc = true;
  }

  return rc;
}

bool State::leaving() { return i.s.mode == Mode::Leaving; }

void State::quit() { i.s.mode = Mode::Quitting; }
bool State::quitting() { return i.s.mode == Mode::Quitting; }

void State::shutdown() { i.s.mode = Mode::Shutdown; }

} // namespace pierre
