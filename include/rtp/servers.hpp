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

#include "packet/queued.hpp"
#include "rtp/tcp/audio/server.hpp"
#include "rtp/tcp/event/server.hpp"
#include "rtp/udp/control/server.hpp"

namespace pierre {
namespace rtp {

enum ServerType : uint8_t { Audio = 0, Control, Event };

class Servers {
public:
  using AudioServer = rtp::tcp::AudioServer;
  using EventServer = rtp::tcp::EventServer;
  using ControlServer = rtp::udp::ControlServer;
  using io_context = boost::asio::io_context;
  using error_code = boost::system::error_code;
  using enum ServerType;

  typedef std::variant<AudioServer, ControlServer, EventServer> Variant;
  typedef std::unordered_map<ServerType, Variant> Map;

public:
  struct Opts {
    io_context &io_ctx;
    packet::Queued &audio_raw;
  };

public:
  Servers(const Opts &opts);

  AudioServer &audio();
  ControlServer &control();
  EventServer &event();

  void teardown();

private:
  Variant &getOrCreate(ServerType type);

private:
  // order dependent based on constructor
  io_context &io_ctx;
  packet::Queued &audio_raw;

  // order independent
  Map map;
};

} // namespace rtp
} // namespace pierre
