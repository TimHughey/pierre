
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
#include <memory>

#include "event/receiver.hpp"

namespace pierre {

// forward decl for shared_ptr typedef
class Rtp;
typedef std::shared_ptr<Rtp> sRtp;

class Rtp : public std::enable_shared_from_this<Rtp> {

public:
  ~Rtp();

public: // object creation and shared_ptr API
  [[nodiscard]] static sRtp create() {
    // not using std::make_shared; constructor is private
    return sRtp(new Rtp());
  }

  sRtp getPtr() { return shared_from_this(); }

public:
  void start() {}
  rtp::event::PortFuture startEventReceiver() {
    return _event_receiver->start();
  }
  // Public API

private:
  Rtp();

private:
  rtp::event::sReceiver _event_receiver;
  bool running = false;
  uint64_t last_resend_request_error_ns = 0;
};

} // namespace pierre