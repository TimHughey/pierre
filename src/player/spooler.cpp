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

#include "player/spooler.hpp"
#include "player/flush_request.hpp"
#include "player/frame.hpp"
#include "player/reel.hpp"
#include "player/typedefs.hpp"

#include <algorithm>
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <fmt/format.h>
#include <iterator>
#include <mutex>
#include <optional>
#include <ranges>
#include <vector>

using namespace std::chrono_literals;
using error_code = boost::system::error_code;

namespace pierre {
namespace player {
namespace asio = boost::asio;
namespace errc = boost::system::errc;
namespace ranges = std::ranges;

// general API and member functions

void Spooler::flush(const FlushRequest &request) {
  asio::post(strand_in, // serialize Reels reels_in actions
             [self = shared_from_this(), request = request]() {
               self->flushReels(request, self->reels_in);
             });

  asio::post(strand_out, // serialize Reels reels_in actions
             [self = shared_from_this(), request = request]() {
               self->flushReels(request, self->reels_out);
             });
}

shFrame Spooler::nextFrame(const FrameTimeDiff &ftd) {
  // do we need a reel?
  requisition.ifNeeded();

  shFrame frame;

  // look through all available reels for the next frame
  // no need to capture the result, frame is set within the find
  ranges::find_if(reels_out, [&frame = frame, &ftd = ftd](shReel reel) {
    frame = reel->nextFrame(ftd);

    // if frame is ok then stop finding, otherwise keep searching
    return Frame::ok(frame);
  });

  // clean up empty reels
  auto erased = std::erase_if(reels_out, [](shReel reel) { return reel->empty(); });

  if (erased) {
    __LOG0("{:<18} reels erased={}\n", moduleId(), erased);
  }

  return Frame::ok(frame) ? frame->shared_from_this() : frame;

  // return f != reels_out.end() ? frame->shared_from_this() : shFrame();
}

shFrame Spooler::queueFrame(shFrame frame) {
  asio::post(strand_in, // guard with reels strand
             [&dst = reels_in, frame = frame]() {
               shReel dst_reel = dst.empty() //
                                     ? dst.emplace_back(Reel::create())
                                     : dst.back()->shared_from_this();

               dst_reel->addFrame(frame);
             });

  return frame;
}

// misc debug, stats

const string Spooler::inspect() const {
  const auto &INDENT = __LOG_MODULE_ID_INDENT;

  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "{:<12} load={:<3} unload={:<3}\n", csv("REEL"), reels_in.size(),
                 reels_out.size());

  ranges::for_each(reels_in, [w = w](shReel reel) {
    fmt::format_to(w, "{} {:<12} {}\n", INDENT, reel->moduleId(), Reel::inspect(reel));
  });

  ranges::for_each(reels_out, [w = w](shReel reel) {
    fmt::format_to(w, "{} {:<12} {}\n", INDENT, reel->moduleId(), Reel::inspect(reel));
  });

  return msg;
}

} // namespace player
} // namespace pierre
