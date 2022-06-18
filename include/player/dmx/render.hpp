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

#include "core/typedefs.hpp"
#include "dmx/producer.hpp"
#include "player/frames_request.hpp"
#include "player/rtp.hpp"
#include "player/spooler.hpp"
#include "player/typedefs.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <set>

namespace pierre {
namespace dmx {

namespace { // anonymous namespace limits scope to this file
namespace asio = boost::asio;
}

using FPS = std::chrono::duration<double, std::ratio<1, 44>>;

// duration to wait between sending DMX frames
constexpr Nanos fps_ns() { return std::chrono::duration_cast<Nanos>(FPS(1)); }
// constexpr Nanos fps_ns() { return std::chrono::duration_cast<Nanos>(MillisFP(43.0065)); }

class Render;
typedef std::shared_ptr<Render> shRender;

class Render : public std::enable_shared_from_this<Render> {
public:
  typedef std::set<std::shared_ptr<Producer>> Producers;

private: // all access via shared_ptr
  Render(asio::io_context &io_ctx, player::shFramesRequest frames_request);

public: // static creation, access, etc.
  static shRender init(asio::io_context &io_ctx, player::shFramesRequest frames_request);
  static shRender ptr();
  static void reset();

public: // public API
  void addProducer(std::shared_ptr<Producer> producer) { _producers.insert(producer); }
  static void flush(const FlushRequest &request);
  static void playMode(bool mode);
  static void teardown();

private:
  void frameTimer();
  shRender handleFrame();
  static void nextPacket(player::shRTP next_packet, const Nanos start);

  bool playing() const { return _play_mode == player::PLAYING; }

  const string stats() const;
  void statsTimer(const Nanos report_ns = 30s);

private:
  // order dependent
  asio::io_context::strand local_strand;
  asio::high_resolution_timer frame_timer;
  asio::high_resolution_timer stats_timer;
  player::shSpooler spooler;
  player::shFramesRequest frames_request;

  // order independent
  bool _play_mode = player::NOT_PLAYING;
  player::shRTP _recent_frame;
  uint64_t _frames_played = 0;
  uint64_t _frames_silence = 0;

  Producers _producers;

  static constexpr auto moduleId = csv("RENDER");
};
} // namespace dmx
} // namespace pierre
