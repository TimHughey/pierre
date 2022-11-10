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

#include "base/types.hpp"
#include "desk/data_msg.hpp"
#include "desk/unit.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <type_traits>

namespace pierre {

using units_map = std::map<string, std::shared_ptr<Unit>>;

class Units {

public:
  Units() = default;

  template <typename T> void add(unit::Opts opts) noexcept {
    _units_map.try_emplace(string(opts.name), std::make_shared<T>(opts));
  }

  void dark() noexcept {
    for (auto it = _units_map.begin(); it != _units_map.end(); it++) {
      it->second->dark();
    }
  }

  bool empty() const noexcept { return _units_map.empty(); }

  template <typename T> auto operator()(csv name) noexcept { return get<T>(name); }

  template <typename T> auto get(csv name) noexcept { return get<T>(string(name)); }

  template <typename T> auto get(string name) noexcept {
    return static_pointer_cast<T>(_units_map[name]);
  }

  void leave() noexcept {
    for (auto it = _units_map.begin(); it != _units_map.end(); it++) {
      it->second->leave();
    }
  }

  void prepare() noexcept {
    for (auto it = _units_map.begin(); it != _units_map.end(); it++) {
      it->second->prepare();
    }
  }
  void update_msg(desk::DataMsg &m) noexcept {
    for (auto it = _units_map.begin(); it != _units_map.end(); it++) {
      it->second->update_msg(m);
    }
  }

private:
  static units_map _units_map;
};

} // namespace pierre