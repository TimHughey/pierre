
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

#include "base/flush_request.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"

#include <deque>
#include <iterator>
#include <memory>
#include <ranges>

namespace pierre {

// a sequence of frames in ascending order
class Reel;
typedef std::shared_ptr<Reel> shReel;

// a reel contains frames, in ascending order by sequence,
// possibly with gaps
typedef std::vector<shReel> Reels;
typedef std::deque<shFrame> Frames;

class Reel : public std::enable_shared_from_this<Reel> {
private: // constructor private, all access through shared_ptr
  Reel(strand &strand_out)
      : strand_out(strand_out),                       // note 1
        serial(fmt::format("{:#05x}", ++SERIAL_NUM)), // note 2
        module_id(fmt::format("REEL {}", serial))     // note 3
  {
    // notes:
    //  1. Reel unique serial num (for debugging)
    //  1. Reel loggind prefix
    //  2. initialize stats map to capture the status of the frames
    //     returned by nextFrame()
  }

  // static functions that create Reels
public:
  static shReel create(strand &strand_out) { return shReel(new Reel(strand_out)); }

public:
  void addFrame(shFrame frame) { _frames.emplace_back(frame); }

  bool empty() const { return _frames.empty(); }
  bool flush(const FlushRequest &flush);
  Frames &frames() { return _frames; }

  void purge(); // erase purgeable frames

  const string &serialNum() const { return serial; }
  size_t size() const { return _frames.size(); }

  // misc stats, debug
  static const string inspect(shReel reel);
  void log() { __LOG(LCOL01 "{}", moduleID(), "INSPECT", inspect(shared_from_this())); }
  const string &moduleID() const { return module_id; }

private:
  // order dependent
  strand &strand_out;
  const string serial;
  const string module_id;

  // order independent
  Frames _frames;

  // class static
  static uint64_t SERIAL_NUM;
};

} // namespace pierre
