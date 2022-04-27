
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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#pragma once

#include <boost/asio/io_context.hpp>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "rtp/anchor_info.hpp"
#include "rtp/audio/buffered/server.hpp"
#include "rtp/control/datagram.hpp"
#include "rtp/event/server.hpp"
#include "rtp/input_info.hpp"
#include "rtp/stream_info.hpp"
#include "rtp/timing/datagram.hpp"

namespace pierre {

// forward decl for shared_ptr typedef
class Rtp;
typedef std::shared_ptr<Rtp> sRtp;

enum ServerType : uint8_t { AudioBuffered = 0, Event, Control, Timing };

class Rtp : public std::enable_shared_from_this<Rtp> {
private:
  struct Servers {
    rtp::event::sServer event;
    rtp::audio::buffered::sServer audio_buffered;
    rtp::control::sDatagram control;
    rtp::timing::sDatagram timing;
  };

public:
  using io_context = boost::asio::io_context;
  using string = std::string;
  typedef const string &csr;

public: // object creation and shared_ptr API
  [[nodiscard]] static sRtp create() {
    if (_instance.use_count() == 0) {
      _instance = sRtp(new Rtp());
    }
    // not using std::make_shared; constructor is private
    return _instance;
  }

  [[nodiscard]] static sRtp instance() { return create(); }

  sRtp getSelf() { return shared_from_this(); }

  void static shutdown() { _instance.reset(); }

public:
  // Public API
  size_t bufferFrames() const { return 1024; }
  size_t bufferStartFill() const { return 220; }
  uint16_t localPort(ServerType type); // local endpoint port
  void start();

  // Savers
  void save(const rtp::AnchorData &anchor_data);
  void save(const rtp::StreamData &stream_data);

  // Getters
  size_t bufferSize() const { return 1024 * 1024 * 8; };

private:
  Rtp();

  void runLoop();

private:
  io_context io_ctx;
  // shared pointers to all servers spun up for RTP
  Servers servers;

  // order independent
  uint32_t _frames_per_packet_max = 352; // audio frames per packet
  uint32_t _backend_latency = 0;
  uint64_t _rate;

  // runtime info
  rtp::StreamInfo _stream_info;
  rtp::AnchorInfo _anchor;
  rtp::InputInfo _input_info;

  bool running = false;
  uint64_t last_resend_request_error_ns = 0;

  static std::shared_ptr<Rtp> _instance;
  std::jthread _thread;
};

} // namespace pierre