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
#include "packet/dmx.hpp"

#include <exception>
#include <memory>
#include <ranges>
#include <vector>

namespace pierre {
namespace {
namespace ranges = std::ranges;
}

class Desk;
typedef std::shared_ptr<Desk> shDesk;

class Desk : public std::enable_shared_from_this<Desk> {
private:
  typedef std::vector<shHeadUnit> Units;

private:
  Desk() = default;

public:
  // static create, access to Desk
  static shDesk create();
  template <typename T> static std::shared_ptr<T> createUnit(auto opts) {
    return ptr()->addUnit(opts);
  }
  static shDesk ptr();

  // std::shared_ptr<fx::FX> activeFX() const { return _active.fx; }

  // void update(packet::DMX &packet) override {
  //   executeFX();
  //   packet.rootObj()["ACP"] = true; // AC power on
  //   _tracker->update(packet);
  // }

public:
  template <typename T> std::shared_ptr<T> addUnit(const auto opts) {
    return std::static_pointer_cast<T>(units.emplace_back(std::make_shared<T>(opts)));
  }

  static void dark() {
    forEach([](shHeadUnit unit) { unit->dark(); });
  }

  static void leave() {
    forEach([](shHeadUnit unit) { unit->leave(); });
  }

  static void prepare() {
    forEach([](shHeadUnit unit) { unit->framePrepare(); });
  }

  static shHeadUnit unit(csv name) {
    const auto &units = ptr()->units;

    auto unit = ranges::find_if(units, [&name](shHeadUnit u) { return name == u->unitName(); });

    if (unit != units.end()) [[likely]] {
      return *unit;
    } else {
      static string msg;
      fmt::format_to(std::back_inserter(msg), "unit [{}] not found", name);
      throw(std::runtime_error(msg.c_str()));
    }
  }

  template <typename T> static std::shared_ptr<T> unitDerived(csv name) {
    return std::static_pointer_cast<T>(unit(name));
  }

  static void update(packet::DMX &packet) {
    forEach([&packet](shHeadUnit unit) { unit->frameUpdate(packet); });
  }

private:
  static void forEach(auto func) { ranges::for_each(ptr()->units, func); }

private:
  Units units;

  // struct {
  //   std::mutex mtx;
  //   std::shared_ptr<fx::FX> fx;
  // } _active;

  // spPinSpot main;
  // spPinSpot fill;
  // spLedForest led_forest;
  // spElWire el_dance_floor;
  // spElWire el_entry;
  // spDiscoBall discoball;
};

} // namespace pierre
