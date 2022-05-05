
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
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "anchor/anchor.hpp"
#include "core/input_info.hpp"
#include "decouple/conn_info.hpp"
#include "decouple/stream_info.hpp"
#include "packet/queued.hpp"
#include "pcm/pcm.hpp"
#include "rtp/servers.hpp"

namespace pierre {

// forward decl for shared_ptr typedef
class Rtp;
typedef std::shared_ptr<Rtp> sRtp;

class Rtp : public std::enable_shared_from_this<Rtp> {
public:
  enum TeardownPhase : uint8_t { None = 0, One, Two };

public:
  using io_context = boost::asio::io_context;
  using string = std::string;
  using WatchDog = boost::asio::high_resolution_timer;

  typedef std::future<TeardownPhase> TeardownBarrier;
  typedef std::promise<TeardownPhase> Teardown;
  typedef const string &csr;
  typedef std::optional<Teardown> TeardownKeeper;
  typedef const char *ccs;

public: // object creation and shared_ptr API
  [[nodiscard]] static sRtp create() {
    if (_instance.use_count() == 0) {
      _instance = sRtp(new Rtp());
    }
    // not using std::make_shared; constructor is private
    return _instance;
  }

  [[nodiscard]] static sRtp instance() { return _instance; }

  sRtp getSelf() { return shared_from_this(); }

  void static shutdown() { _instance.reset(); }

public:
  // Public API
  size_t bufferFrames() const { return 1024; }
  size_t bufferStartFill() const { return 220; }
  uint16_t localPort(rtp::ServerType type); // local endpoint port

  void save(const StreamData &stream_data);

  void start();
  [[nodiscard]] TeardownBarrier teardown(TeardownPhase phase = TeardownPhase::Two);

  // Getters
  size_t bufferSize() const { return 1024 * 1024 * 8; };

private:
  Rtp();

  void runLoop();
  void watchForTeardown(WatchDog &watch_dog);
  void teardownNow();
  void teardownFinished();

  // misc helpers
  static ccs fnName(std::source_location loc = std::source_location::current()) {
    return loc.function_name();
  }

private:
  // order dependent
  packet::Queued audio_raw;
  io_context io_ctx;
  rtp::Servers servers; // all servers spun up for RTP
  sPulseCodeMod pcm;    // PCM processor

  // order independent
  uint32_t _frames_per_packet_max = 352; // audio frames per packet
  uint32_t _backend_latency = 0;
  uint64_t _rate;

  // runtime info
  StreamInfo _stream_info;

  InputInfo _input_info;

  TeardownKeeper _teardown;
  TeardownPhase _teardown_phase = None;

  bool running = false;
  uint64_t last_resend_request_error_ns = 0;

  static std::shared_ptr<Rtp> _instance;
  std::jthread _thread;
};

} // namespace pierre