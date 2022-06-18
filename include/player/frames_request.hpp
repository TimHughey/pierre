
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

#include "core/typedefs.hpp"
#include "player/spooler.hpp"
#include "player/typedefs.hpp"
#include "rtp_time/rtp_time.hpp"

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <memory>

namespace pierre {
namespace player {

namespace { // anonymous namespace limits scope
namespace asio = boost::asio;
} // namespace

class FramesRequest;
typedef std::shared_ptr<FramesRequest> shFramesRequest;

class FramesRequest : public std::enable_shared_from_this<FramesRequest> {
private: // all access via shared_ptr
  FramesRequest() = default;

public:
  static shFramesRequest create();

public:
  size_t at_least_frames = 0;
  shSpooler src;
  shSpooler dest;

public:
  // returns true if request was queued, false if request pending
  bool atLeastFrames(size_t frames);
  bool complete() const { return _complete; }
  template <typename T> T elapsed() { return std::chrono::duration_cast<T>(_elapsed_ns); }
  void finish(shSpool spool);
  bool pending() const { return _complete == false; }

private:
  std::atomic_bool _complete = true; // default to true
  Nanos _at_ns{0};
  Nanos _elapsed_ns{0};
};

} // namespace player
} // namespace pierre