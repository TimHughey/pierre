
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
#include "base/pe_time.hpp"
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

class Reel : public std::enable_shared_from_this<Reel> {
private:
  typedef std::deque<shFrame> Frames;

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
  void addFrame(shFrame frame) { frames.emplace_back(frame); }
  bool empty() const { return frames.empty(); }
  bool flush(const FlushRequest &flush);

  // get and return the next frame
  //
  // notes:
  //   1. an empty shFrame will be returned if a frame was not found
  //      this is a signal to the caller to keep looking in other reels
  //   2. if a frame is found it may not be playable
  //      handling unplayable frames is left to the caller
  shFrame nextFrame(const Nanos &lead_time) {
    auto next =
        ranges::find_if(frames, [&](shFrame frame) { return frame->nextFrame(lead_time); });

    // schedule reel cleanup and guard by strand_out
    asio::post(strand_out, [self = shared_from_this()]() {
      std::erase_if(self->frames, [](shFrame frame) { return frame->purgeable(); });
    });

    return next != frames.end() ? (*next)->shared_from_this() : shFrame();
  }

  const string &serialNum() const { return serial; }
  size_t size() const { return frames.size(); }

  bool unplayedAtLeastOne() const { return unplayedCount() > 0; }

  size_t unplayedCount() const {
    return ranges::count_if(frames, [](auto frame) { return frame->unplayed(); });
  }

  // misc stats, debug
  static const string inspect(shReel reel);
  void log() { __LOG("{:<18} {}", moduleId(), inspect(shared_from_this())); }
  const string &moduleId() const { return module_id; }

private:
  // order dependent
  strand &strand_out;
  const string serial;
  const string module_id;

  // order independent
  Frames frames;

  // class static
  static uint64_t SERIAL_NUM;
  static constexpr size_t PURGE_MAX = 10;
};

} // namespace pierre
