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

#include <exception>

#include "rtp/servers.hpp"

namespace pierre {
namespace rtp {

using io_context = boost::asio::io_context;
using AudioServer = rtp::tcp::AudioServer;
using EventServer = rtp::tcp::EventServer;
using ControlServer = rtp::udp::ControlServer;

Servers::Servers(const Opts &opts) : io_ctx(opts.io_ctx), audio_raw(opts.audio_raw) {}

AudioServer &Servers::audio() {
  auto &variant = getOrCreate(Audio);

  return std::get<AudioServer>(variant);
}

ControlServer &Servers::control() {
  auto &variant = getOrCreate(Control);

  return std::get<ControlServer>(variant);
}

EventServer &Servers::event() {
  auto &variant = getOrCreate(Event);

  return std::get<EventServer>(variant);
}

Servers::Variant &Servers::getOrCreate(ServerType type) {
  switch (type) {
    case Audio:
      map.try_emplace(type, Variant(AudioServer({.io_ctx = io_ctx, .audio_raw = audio_raw})));
      break;

    case Control:
      map.try_emplace(type, Variant(ControlServer(io_ctx)));
      break;

    case Event:
      map.try_emplace(type, Variant(EventServer(io_ctx)));
      break;
  }

  return map.at(type);
}

void Servers::teardown() {
  for (auto &[key, variant] : map) {
    std::visit([](auto &&server) { server.teardown(); }, variant);
  }

  map.clear();
}

} // namespace rtp
} // namespace pierre