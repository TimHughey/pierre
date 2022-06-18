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

#include "player/frames_request.hpp"
#include "player/rtp.hpp"
#include "player/typedefs.hpp"

#include <algorithm>
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <fmt/format.h>
#include <ranges>

using namespace std::chrono_literals;
using error_code = boost::system::error_code;

namespace pierre {
namespace player {
namespace asio = boost::asio;
namespace errc = boost::system::errc;
namespace ranges = std::ranges;
namespace chrono = std::chrono;

shFramesRequest FramesRequest::create() { return shFramesRequest(new FramesRequest()); }

// Public API and general member functions
bool FramesRequest::atLeastFrames(size_t frames) {
  // runs within dest strand
  if (_complete == false) {
    return false;
  }

  at_least_frames = frames;
  _complete = false;
  _at_ns = rtp_time::nowNanos();

  // find a spool with enough frames
  asio::post(src->spooler_strand, [r = shared_from_this()]() mutable { // src strand
    auto src = r->src;
    auto dest = r->dest;

    __LOG0("{}\n", src->statsMsg()); // report src stats via src strand

    // are enough frames available?
    if (src->firstSpoolAvailableFrames() >= r->at_least_frames) {
      auto spool = src->_spools.front();
      src->_spools.pop_front(); // src strand

      // move the spool via the dest spooler strand
      asio::post(dest->spooler_strand, [spool = spool, r = r]() mutable { // dest strand
        r->finish(spool);
      });
    }
  });

  return true;
}

void FramesRequest::finish(shSpool spool) {
  dest->queueSpool(spool); // dest strand
  _elapsed_ns = rtp_time::nowNanos() - _at_ns;
  _complete = true;

  __LOG0("{:<18} FINISH at_least_frames={} total_frames={} latency={}\n", //
         csv("FRAME REQUEST"), at_least_frames, spool->framesAvailable(), elapsed<Micros>());
  __LOG0("{}\n", dest->statsMsg()); // report dest stats via dest strand
}

} // namespace player
} // namespace pierre
