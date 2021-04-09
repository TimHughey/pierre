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

#include "core/state.hpp"

namespace pierre {
namespace core {

using namespace std;

State State::i = State();

shared_ptr<Config> State::config() { return i._cfg; }

toml::table *State::config(const string_view &key) {
  return i._cfg->table().get(key)->as_table();
}

toml::table *State::config(const string_view &key, const string_view &sub) {
  return i._cfg->table().get(key)->as_table()->get(sub)->as_table();
}

shared_ptr<Config> State::initConfig() noexcept {
  i._cfg = make_shared<Config>();

  return i._cfg;
}

void State::leave(milliseconds ms) {
  i.s.mode = Leaving;
  i.s.leaving.started = clock::now();
  i.s.leaving.ms = ms;
}

bool State::leaveInProgress() {
  auto rc = false;

  auto elapsed = clock::now() - i.s.leaving.started;

  if (elapsed < i.s.leaving.ms) {
    rc = true;
  }

  return rc;
}

bool State::leaving() { return i.s.mode == Leaving; }

void State::quit() { i.s.mode = Quitting; }
bool State::quitting() { return i.s.mode == Quitting; }
bool State::running() { return i.s.mode != Shutdown; }

bool State::silence() { return i.s.mode == Silence; }
void State::shutdown() { i.s.mode = Shutdown; }

} // namespace core
} // namespace pierre
