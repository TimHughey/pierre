
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
#include <source_location>
#include <string>
#include <thread>

#include "anchor/anchor.hpp"
#include "core/input_info.hpp"
#include "decouple/stream_info.hpp"
#include "packet/basic.hpp"
#include "packet/queued.hpp"

namespace pierre {

// forward decl for shared_ptr typedef
class PulseCodeMod;
typedef std::shared_ptr<PulseCodeMod> sPulseCodeMod;

class PulseCodeMod : public std::enable_shared_from_this<PulseCodeMod> {
private:
  using src_loc = std::source_location;
  using string = std::string;
  typedef const char *ccs;
  typedef unsigned long long int ullong_t;
  typedef const src_loc csrc_loc;

public:
  struct Opts {
    packet::Queued &audio_raw;
    StreamInfo &stream_info;
  };

public: // object creation and shared_ptr API
  [[nodiscard]] static sPulseCodeMod create(const Opts &opts) {
    if (_instance_.use_count() == 0) {
      _instance_ = sPulseCodeMod(new PulseCodeMod(opts));
    }
    // not using std::make_shared; constructor is private
    return _instance_;
  }

  [[nodiscard]] auto instance() { return _instance_->shared_from_this(); }

  sPulseCodeMod getSelf() { return shared_from_this(); }

  // void static shutdown() { _instance.reset(); }

public:
  // public API

private:
  PulseCodeMod(const Opts &opts);

  void runLoop();

  // misc helpers
  static ccs fnName(csrc_loc loc = src_loc::current()) { return loc.function_name(); }

private:
  // order dependent
  packet::Queued &queued;
  StreamInfo &stream_info;

  packet::Basic buffer;

  size_t rx_bytes = 0;

  bool running = false;

  static std::shared_ptr<PulseCodeMod> _instance_;
  std::jthread thread;
};

} // namespace pierre