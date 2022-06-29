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

#include "headunit/tracker.hpp"
#include "base/typical.hpp"
#include "headunit/headunit.hpp"

#include <map>
#include <memory>
#include <ranges>

namespace ranges = std::ranges;

namespace pierre {
namespace hdu {

void Tracker::dark() {
  ranges::for_each(map, [](auto x) {
    auto unit = std::get<1>(x);
    unit->dark();
  });
}

template <typename T> void insert(const string name, uint address = 0) {
  auto pair = std::make_pair(name, std::make_shared<T>(address));

  _map->insert(pair);
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
HeadUnitMap _map = std::make_shared<_HeadUnitMap>();
};

typedef std::shared_ptr<Tracker> spTracker;

} // namespace hdu
} // namespace pierre
