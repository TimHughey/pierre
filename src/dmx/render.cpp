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

#include "dmx/render.hpp"
#include "core/state.hpp"
#include "external/ArduinoJson.h"

typedef StaticJsonDocument<1024> doc;

using std::cerr, std::endl, std::vector;
using std::chrono::duration;
using std::chrono::duration_cast;
using us = std::chrono::microseconds;
using std::chrono::steady_clock;
using std::chrono::time_point;

namespace this_thread = std::this_thread;

namespace pierre {
namespace dmx {

Render::Render(const Config &cfg)
    : _cfg(cfg), _net(_io_ctx, _cfg.host, _cfg.port) {

  _frame.assign(256, 0x00);
}

std::shared_ptr<std::thread> Render::run() {
  auto t = std::make_shared<std::thread>([this]() { this->stream(); });
  return t;
}

void Render::stream() {
  string_t func(__PRETTY_FUNCTION__);

  // this_thread::sleep_for(chrono::seconds(2));

  uint64_t frames = 0;
  long frame_us = ((1000 * 1000) / 44) - 250;
  duration frame_interval_half = us(frame_us / 2);

  while (core::State::running()) {
    auto loop_start = steady_clock::now();
    for (auto p : _producers) {
      p->prepare();
    }

    this_thread::sleep_for(frame_interval_half);

    dmx::Packet packet;

    for (auto p : _producers) {
      p->update(packet);
    }

    auto elapsed = duration_cast<us>(steady_clock::now() - loop_start);

    this_thread::sleep_for(us(frame_us) - elapsed);

    _net.write(packet);

    frames++;
  }
}
} // namespace dmx

} // namespace pierre
