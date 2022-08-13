/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#include "base/typical.hpp"
#include "desk/headunit.hpp"
#include "desk/msg.hpp"

#include <algorithm>
#include <memory>
#include <ranges>
#include <vector>

namespace pierre {

class HeadUnits;
typedef std::shared_ptr<HeadUnits> shHeadUnits;

class HeadUnits : public std::vector<shHeadUnit>, std::enable_shared_from_this<HeadUnits> {

public:
  HeadUnits() = default;
  ~HeadUnits() = default;

  template <typename T> void add(auto opts) { this->emplace_back(std::make_shared<T>(opts)); }

  void dark() {
    ranges::for_each(*this, [](auto unit) { unit->dark(); });
  }

  template <typename T> std::shared_ptr<T> derive(csv name) {
    return std::static_pointer_cast<T>(find(name));
  }

  shHeadUnit find(csv name) {
    auto check_name = [=](shHeadUnit u) { return name == u->unitName(); };

    if (auto unit = ranges::find_if(*this, check_name); unit != this->end()) {
      return *unit;
    }

    else {
      static string msg;
      fmt::format_to(std::back_inserter(msg), "unit [{}] not found", name);
      throw(std::runtime_error(msg.c_str()));
    }
  }

  void leave() {
    ranges::for_each(*this, [](auto unit) { unit->leave(); });
  }

  void prepare() {
    ranges::for_each(*this, [](auto unit) { unit->prepare(); });
  }

  void update_msg(desk::shMsg msg) {
    ranges::for_each(*this, [=](auto unit) { unit->update_msg(msg); });
  }
};

} // namespace pierre