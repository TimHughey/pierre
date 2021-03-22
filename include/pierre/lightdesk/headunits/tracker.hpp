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

#ifndef pierre_lightdesk_headunit_tracker_hpp
#define pierre_lightdesk_headunit_tracker_hpp

#include <memory>
#include <unordered_map>

#include "lightdesk/headunit.hpp"
#include "lightdesk/headunits/discoball.hpp"
#include "lightdesk/headunits/elwire.hpp"
#include "lightdesk/headunits/ledforest.hpp"
#include "lightdesk/headunits/pinspot/pinspot.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

typedef std::shared_ptr<HeadUnit> _HeadUnit;
typedef std::unordered_map<string_t, _HeadUnit> _HeadUnitMap;
typedef std::shared_ptr<_HeadUnitMap> HeadUnitMap;

class HeadUnitTracker {
public:
  HeadUnitTracker() = default;
  ~HeadUnitTracker() = default;

  HeadUnitTracker(const HeadUnitTracker &) = delete;
  HeadUnitTracker &operator=(const HeadUnitTracker &) = delete;

  template <typename T> std::shared_ptr<T> find(const string_t name) {
    auto search = _map->find(name);
    auto x = std::static_pointer_cast<T>(std::get<1>(*search));
    return x;
  }

  template <typename T> void insert(const string_t name, uint address = 0) {
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

  template <typename T> std::shared_ptr<T> unit(const string_t name) {
    auto search = _map->find(name);
    auto x = std::static_pointer_cast<T>(std::get<1>(*search));
    return x;
  }

  void update(dmx::Packet &packet) {
    for (auto t : *_map) {
      auto headunit = std::get<1>(t);

      headunit->frameUpdate(packet);
    }
  }

private:
  HeadUnitMap _map = std::make_shared<_HeadUnitMap>();
};

} // namespace lightdesk
} // namespace pierre

#endif
