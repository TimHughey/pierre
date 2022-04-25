
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

#include <cstdint>
#include <future>
#include <memory>
#include <string>

#include "rtp/anchor_info.hpp"
#include "rtp/buffered.hpp"
#include "rtp/control.hpp"
#include "rtp/event.hpp"
#include "rtp/input_info.hpp"

namespace pierre {

// forward decl for shared_ptr typedef
class Rtp;
typedef std::shared_ptr<Rtp> sRtp;

class Rtp : public std::enable_shared_from_this<Rtp> {
public:
  using string = std::string;
  typedef const string &csr;

public:
  ~Rtp();

public: // object creation and shared_ptr API
  [[nodiscard]] static sRtp create() {
    // not using std::make_shared; constructor is private
    return sRtp(new Rtp());
  }

  sRtp getSelf() { return shared_from_this(); }

public:
  void start() {}

  size_t bufferSize() const { return 1024 * 1024 * 8; };

  // Public API
  void saveAnchorInfo(const rtp::AnchorData &anchor_data);
  void saveSessionInfo(csr shk, csr active_remote, csr dacp_id);

  rtp::PortFuture startBuffered() { return _buffered->start(); }
  rtp::PortFuture startControl() { return _control->start(); }
  rtp::PortFuture startEvent() { return _event->start(); }

private:
  Rtp();

private:
  // order dependent for constructor
  rtp::sEvent _event;
  rtp::sControl _control;
  rtp::sBuffered _buffered;

  // order independent
  rtp::InputInfo _input_info;
  uint32_t _frames_per_packet_max = 352; // audio frames per packet
  string _session_key;
  string _active_remote;
  string _dacp_id;

  uint32_t _backend_latency = 0;
  uint64_t _rate;

  rtp::AnchorInfo _anchor;

  bool running = false;
  uint64_t last_resend_request_error_ns = 0;
};

} // namespace pierre