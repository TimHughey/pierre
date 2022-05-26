//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "common/ss_inject.hpp"
#include "packet/in.hpp"
#include "session/base.hpp"

#include <fmt/format.h>
#include <memory>

namespace pierre {
namespace airplay {
namespace session {

class Event; // forward decl for shared_ptr def
typedef std::shared_ptr<Event> shEvent;

class Event : public Base, public std::enable_shared_from_this<Event> {
public:
  static shEvent start(const Inject &di) {
    // creates the shared_ptr and starts the async loop
    // the asyncLoop holds onto the shared_ptr until an error on the
    // socket is detected
    auto session = shEvent(new Event(di));

    session->asyncLoop();

    return session;
  }

private:
  Event(const Inject &di)
      : Base(di, csv("EVENT SESSION")) // Base holds the newly connected socket
  {}

public:
  // initiates async event run loop
  void asyncLoop() override; // see .cpp file for critical function details

private:
  void handleEvent(size_t bytes);
  void nextEvent();

  bool rxAtLeast(size_t bytes = 1);
  bool rxAvailable(); // load bytes immediately available
  packet::In &wire() { return _wire; }
  void wireToPacket();

private:
  packet::In _wire;
};

} // namespace session
} // namespace airplay
} // namespace pierre