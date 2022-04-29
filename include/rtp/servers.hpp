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

#include <boost/asio.hpp>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <variant>

#include "rtp/audio/buffered/server.hpp"
#include "rtp/control/datagram.hpp"
#include "rtp/event/server.hpp"

namespace pierre {
namespace rtp {

enum ServerKind : uint8_t { Audio = 0, Control, Event };

class ServerDepot {
public:
  using AudioBuffered = rtp::audio::buffered::Server;
  using EventReceiver = rtp::event::Server;
  using ControlDatagram = rtp::control::Datagram;

  using Variant = std::variant<AudioBuffered, EventReceiver, ControlDatagram>;
  using Map = std::unordered_map<ServerKind, Variant>;

  using io_context = boost::asio::io_context;
  using error_code = boost::system::error_code;

  using enum ServerKind;

public:
  ServerDepot(io_context &io_ctx);

private:
  Map map;
};

} // namespace rtp
} // namespace pierre
