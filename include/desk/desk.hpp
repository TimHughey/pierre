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
#include "desk/dmx.hpp"
#include "desk/fx.hpp"
#include "desk/headunit.hpp"
#include "io/io.hpp"

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
  Desk();

public:
  // static create, access to Desk, FX and Unit
  static shDesk create();

  template <typename T> static shFX createFX() {
    return std::static_pointer_cast<T>(std::make_shared<T>());
  }

  template <typename T> static std::shared_ptr<T> createUnit(auto opts) {
    return ptr()->addUnit(opts);
  }

  template <typename T> static std::shared_ptr<T> derivedFX(csv name) {
    return std::static_pointer_cast<T>(ptr()->active_fx);
  }

  template <typename T> static std::shared_ptr<T> derivedUnit(csv name) {
    return std::static_pointer_cast<T>(unit(name));
  }

  static shDesk ptr();

  // void update(packet::DMX &packet) override {
  //   executeFX();
  //   packet.rootObj()["ACP"] = true; // AC power on
  //   _tracker->update(packet);
  // }

public:
  static shFX activeFX() { return ptr()->active_fx; }

  template <typename T> static shFX activateFX(csv &fx_name) {
    return ptr()->activate<T>(fx_name);
  }

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

    auto unit = ranges::find_if(units, [name = name](shHeadUnit u) { //
      return name == u->unitName();
    });

    if (unit != units.end()) [[likely]] {
      return *unit;
    } else {
      static string msg;
      fmt::format_to(std::back_inserter(msg), "unit [{}] not found", name);
      throw(std::runtime_error(msg.c_str()));
    }
  }

  static void update(packet::DMX &packet) {
    forEach([&packet](shHeadUnit unit) { unit->frameUpdate(packet); });
  }

private:
  template <typename T> shFX activate(csv &fx_name) {
    if (active_fx.use_count() && !active_fx->matchName(fx_name)) {
      // shared_ptr empty or name doesn't match, create the FX
      active_fx = createFX<T>();
    }

    return active_fx;
  }

  static void forEach(auto func) { ranges::for_each(ptr()->units, func); }

private:
  Units units;
  shFX active_fx;
};

} // namespace pierre
