
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
#include "player/flush_request.hpp"
#include "player/rtp.hpp"
#include "player/typedefs.hpp"
#include "rtp_time/rtp_time.hpp"

#include <chrono>
#include <deque>
#include <memory>
#include <ranges>

namespace pierre {
namespace player {

// a sequence of frames in ascending order
class Spool;
typedef std::shared_ptr<Spool> shSpool;

class Spool : public std::enable_shared_from_this<Spool> {
private:
  typedef std::deque<shRTP> Frames;

public:
  static constexpr auto AVAILABLE = csv("available");

public:
  static shSpool create(size_t id) { return shSpool(new Spool(id)); }

private: // constructor private, all access through shared_ptr
  Spool(uint64_t id);

public:
  shRTP addFrame(shRTP frame) { return _frames.emplace_back(frame); }
  const auto &back() const { return _frames.back(); }

  bool flush(const FlushRequest &flush);
  size_t framesAvailable() const;
  size_t frameCount() const { return _frames.size(); }
  const auto &front() const { return _frames.front(); }

  csv moduleId() const { return csv(_module_id); }
  shRTP nextFrame(const Nanos lead_ns);

  fra::StatsMap statsCalc() const;
  const string statsMsg() const;

private:
  Frames _frames;

  string _module_id;
};

} // namespace player
} // namespace pierre