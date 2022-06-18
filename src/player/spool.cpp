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

#include "player/spool.hpp"
#include "player/flush_request.hpp"
#include "player/rtp.hpp"
#include "player/typedefs.hpp"
#include "rtp_time/anchor.hpp"
#include "rtp_time/clock.hpp"

#include <algorithm>
#include <chrono>
#include <fmt/format.h>
#include <iterator>
#include <ranges>

using namespace std::chrono_literals;

namespace pierre {
namespace player {
namespace ranges = std::ranges;
namespace chrono = std::chrono;

// general member functions

Spool::Spool(uint64_t id) {
  // create the module id
  fmt::format_to(std::back_inserter(_module_id), "SPOOL {:>5x}", id);
}

bool Spool::flush(const FlushRequest &flush) {
  if (_frames.empty()) { // nothing to flush
    return false;        // don't keep this spool
  }

  // grab how many packets we currently have for debug stats (below)
  [[maybe_unused]] int64_t frames_before = _frames.size();

  const auto fseq_num = flush.until_seq;
  const auto a = _frames.front();
  const auto b = _frames.back();
  // spool contains flush request
  if ((fseq_num > a->seq_num) && (fseq_num <= b->seq_num)) {
    __LOG0("{} FLUSH FOUND seq={:<7} front={:<7} back={}\n", //
           moduleId(), fseq_num, a->seq_num, b->seq_num);

    Frames frames_keep; // frames to keep

    // the flush seq_num is in this spool. iterate the rtp packets
    // swapping over the frames to keep
    ranges::for_each(_frames, [&](shRTP &frame) mutable {
      // this is a frame to keep, swap it into the new frames
      if (frame->seq_num > flush.until_seq) {
        frames_keep.emplace_back(frame->shared_from_this());
      }
    });

    // if any frames were keep then swap with object frames
    if (frames_keep.empty() == false) {
      std::swap(_frames, frames_keep);
      __LOG0("{}\n", statsMsg());
    }
  } else {
    _frames.clear();
    __LOG0("{} DISCARDED frames={} seq_a/b={}/{}\n", //
           moduleId(), frames_before, a->seq_num, b->seq_num);
  }

  return _frames.empty() == false; // keep this spool
}

size_t Spool::framesAvailable() const {
  const auto stats_map = statsCalc();

  return RTP::available(stats_map);
}

shRTP Spool::nextFrame(const Nanos lead_ns) {
  shRTP frame;

  for (shRTP f : _frames) {
    auto frame = f->stateNow(lead_ns);

    if (RTP::playable(frame)) {
      return frame;
    }

    if (frame->future()) { // finished searching
      break;
    }
  }

  return frame;
}

fra::StatsMap Spool::statsCalc() const {
  auto stats_map = RTP::statsMap();

  ranges::for_each(_frames, [&](auto frame) { frame->statsAdd(stats_map); });

  return stats_map;
}

const string Spool::statsMsg() const {
  if (_frames.empty()) {
    return fmt::format("{} <empty>", moduleId());
  }

  string msg;
  auto w = std::back_inserter(msg);

  static int64_t _size_last = 0;
  const int64_t size_now = _frames.size();
  const int64_t diff = size_now - _size_last;
  _size_last = size_now; // save for next diff compare

  const auto &a = _frames.front();
  const auto &b = _frames.back();

  fmt::format_to(
      w, FMT_STRING("{} frames={:<5} diff={:>+6} seq_a/b={:>8}/{:<8} ts_a/b={:>12}/{:<12}"),
      moduleId(), _frames.size(), diff, // prefix, total frames and diff since last stats
      a->seq_num, b->seq_num,           // general stats and seq_nums
      a->timestamp, b->timestamp);      // timestamps

  auto stats_map = statsCalc();

  ranges::for_each(stats_map, [&](auto &key_val) mutable {
    const auto &[key, val] = key_val;

    if (val > 0) {
      fmt::format_to(w, "{}={:<5}", key, val);
    }
  });

  return msg;
}

} // namespace player
} // namespace pierre
