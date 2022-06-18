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
#include "player/frames_request.hpp"
#include "player/rtp.hpp"
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

static std::atomic_uint64_t SERIAL = 0x1000;

Spooler::Spooler(asio::io_context &io_ctx, csv id)
    : spooler_strand(io_ctx) // serialize container actions
{
  fmt::format_to(std::back_inserter(_module_id), "{} SPOOLER", id);
}

shSpooler Spooler::create(asio::io_context &io_ctx, csv id) { // static
  return shSpooler(new Spooler(io_ctx, id));
}

// general API and member functions
shSpool Spooler::add() { return _spools.emplace_back(Spool::create(++SERIAL)); }

size_t Spooler::firstSpoolAvailableFrames() const {
  if (_spools.empty()) {
    return 0;
  }

  return _spools.front()->framesAvailable();
}

void Spooler::flush(const FlushRequest &request) {
  asio::post(spooler_strand, // serialize spools actions
             [self = shared_from_this(), request = request]() mutable {
               auto &spools = self->_spools; // save typing below

               if (spools.empty()) { // nothing to flush
                 return;
               }

               // create new spools containing the desired seq_nums
               Spools spools_keep;

               // transfer un-flushed rtp_packets to new spools
               ranges::for_each(spools, [&](shSpool spool) {
                 auto keep = spool->flush(request); // spool has frames

                 __LOG0("{}\n", spool->statsMsg());

                 if (keep) {
                   spools_keep.emplace_back(spool);
                 }
               });

               // swap kept spools
               std::swap(spools_keep, spools);

               __LOG0("{}\n", self->statsMsg());
             });
}

size_t Spooler::frameCount() const {
  size_t count = 0;

  ranges::for_each(_spools, [&](shSpool spool) { //
    count += spool->frameCount();
  });

  return count;
}

shRTP Spooler::nextFrame(const Nanos lead_ns) {
  shRTP frame;
  std::vector<shSpool> cleanup;

  for (auto spool : _spools) {
    if (spool->framesAvailable()) {
      frame = spool->nextFrame(lead_ns);
    } else {
      cleanup.emplace_back(spool);
    }
  }

  ranges::for_each(cleanup, [&](shSpool spool) {
    if (auto purge = ranges::find(_spools, spool); purge != _spools.end()) {
      shSpool spool = *purge; // reminder: purge is an iterator
      __LOG0("{:<18} purging {}\n", moduleId(), spool->statsMsg());
      _spools.erase(purge); // erase requires an iterator
    }
  });

  if (RTP::empty(frame)) {
    __LOGX("{} no playable frame located\n", moduleId());
  }

  return frame;
}

shRTP Spooler::queueFrame(shRTP frame) {
  asio::post(spooler_strand, [self = shared_from_this(), frame = frame]() mutable {
    auto &spools = self->_spools;
    auto spool = spools.empty() ? self->add() : spools.back();

    spool->addFrame(frame);
  });

  return frame;
}

const string Spooler::statsMsg() const {
  static int64_t frames_last = 0;

  uint32_t seq_a = 0, seq_b = 0;
  uint64_t ts_a = 0, ts_b = 0;

  if (_spools.empty() == false) { // spools could be empty
    const auto &rtp_a = _spools.front()->front();
    const auto &rtp_b = _spools.back()->back();

    seq_a = rtp_a->seq_num;
    seq_b = rtp_b->seq_num;

    ts_a = rtp_a->timestamp;
    ts_b = rtp_b->timestamp;
  }

  const int64_t frames_now = frameCount();
  const int64_t diff = frames_now - frames_last;
  frames_last = frames_now; // record the current size for delta calc

  return fmt::format(
      "{:<18} spools={:02} frames={:<5} diff={:>+6} seq_a/b={:>8}/{:<8} ts_a/b={:>12}/{:<12}",
      moduleId(),                       // standard logging prefix
      _spools.size(), frames_now, diff, // spool count, total frames and diff
      seq_a, seq_b,                     // general stats and seq_nums
      ts_a, ts_b);                      // timestamps
}

void Spooler::statsAsync() {
  asio::post(spooler_strand, [self = shared_from_this()]() { __LOG0("{}\n", self->statsMsg()); });
}

} // namespace player
} // namespace pierre
