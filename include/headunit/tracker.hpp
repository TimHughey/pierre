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
#include "headunit/headunit.hpp"

#include <exception>
#include <list>
#include <memory>
#include <ranges>

namespace {
namespace ranges = std::ranges;
}

namespace pierre {
namespace hdu {

class Tracker;
typedef std::shared_ptr<Tracker> shTracker;

class Tracker : public std::enable_shared_from_this<Tracker> {
private:
  typedef std::list<HeadUnitAny> HeadUnitList;

private:
  Tracker() = default;

public:
  static shTracker create();

  void dark() {}

  template <typename T> &find(csv name) {
    auto &unit = //
        ranges::find_if(headunits,
                        [](HeadUnitAny &hua) { //
                          return std::any_cast<T &>(hua).unitName() == name;
                        });

    return *unit;
  }

  void leave() {
    std::for_each(_map->begin(), _map->end(), [](auto x) {
      auto unit = std::get<1>(x);
      unit->leave();
    });
  }

  HeadUnitMap map() { return _map; }

  void prepare() {
    std::for_each(_map->begin(), _map->end(), [](auto x) {
      auto unit = std::get<1>(x);
      unit->framePrepare();
    });
  }

  template <typename T> std::shared_ptr<T> unit(const string name) {
    auto search = _map->find(name);
    auto x = std::static_pointer_cast<T>(std::get<1>(*search));
    return x;
  }

  void update(packet::DMX &packet) {
    for (auto t : *_map) {
      auto headunit = std::get<1>(t);

      headunit->frameUpdate(packet);
    }
  }

private:
  void allUnits(auto action) {
    ranges::for_each(headunits, [](HeadUnitAny &hua) { auto T &unit = std::any_cast<T &>(hua); });
  }

  auto T &get(csv name) {
    auto found = ranges::find_if(headunits,
                                 [](HeadUnitAny &hua) { //
                                   return std::any_cast<T &>(hua).unitName() == name;
                                 });

    return *found;
  }

private:
  HeadUnitList headunits;
};

} // namespace hdu
} // namespace pierre
