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
#include "external/ArduinoJson.h"

typedef StaticJsonDocument<1024> doc;

using std::cerr, std::endl, std::vector;
namespace this_thread = std::this_thread;
namespace chrono = std::chrono;

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
  chrono::duration frame_interval_half = chrono::microseconds(frame_us / 2);
  // chrono::time_point loop_start = chrono::steady_clock::now();

  while (_shutdown == false) {
    for (auto p : _producers) {
      p->prepare();
    }

    this_thread::sleep_for(frame_interval_half);

    MsgPackDoc doc;
    JsonObject root = doc.to<JsonObject>();
    root["ACP"] = true; // AC power on

    UpdateInfo info{.frame = _frame, .doc = doc, .obj = root};

    for (auto p : _producers) {
      p->update(info);
    }

    this_thread::sleep_for(frame_interval_half);

    _net.write(info);

    // auto loop_us = chrono::duration_cast<chrono::microseconds>(
    //                    chrono::steady_clock::now() - loop_start)
    //                    .count();

    // if ((loop_us < 21000) || (loop_us > 23800)) {
    //   double ms = double(loop_us) / 1000.0;
    //   cerr.unsetf(std::ios::floatfield);
    //   cerr.precision(4);
    //   cerr << func << " " << ms << "ms ";
    //   cerr << "frame=" << frames << endl;
    // }

    frames++;
  }
  cerr << "Dmx::stream() exiting, shutting down and closing socket" << endl;
}
} // namespace dmx

} // namespace pierre
