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

#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "common/ss_inject.hpp"
#include "session/base.hpp"

#include <memory>
#include <optional>

namespace pierre {
namespace airplay {
namespace session {

class Audio : public Base, public std::enable_shared_from_this<Audio> {
public:
  static auto start(const Inject &di) {
    // creates the shared_ptr and starts the async loop
    // the asyncLoop holds onto the shared_ptr until an error on the
    // socket is detected
    auto session = std::shared_ptr<Audio>(new Audio(di));

    session->asyncLoop();

    return session;
  }

private:
  Audio(const Inject &di) noexcept : Base(di, module_id) {}

public:
  // initiates async audio buffer loop
  void asyncLoop() override; // see .cpp file for critical function details

private:
  void asyncRxPacket();
  uint16_t packetLength();
  void stats();

private:
  // order independent
  uint8v packet_len_buffer;
  uint8v packet_buffer;

public:
  static constexpr csv module_id{"AUDIO_SESSION"};
};

} // namespace session
} // namespace airplay
} // namespace pierre