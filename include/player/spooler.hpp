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
#include "player/reel.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

namespace ranges = std::ranges;

namespace pierre {
namespace player {

class Spooler;
typedef std::shared_ptr<Spooler> shSpooler;

class Spooler : public std::enable_shared_from_this<Spooler> {
private:
  // a reel contains frames, in ascending order by sequence,
  // possibly with gaps
  typedef std::vector<shReel> Reels;

  class Requisition {
    static constexpr size_t FRAMES_MIN = 64;
    static constexpr size_t FRAMES_MAX = FRAMES_MIN * 1.5;
    static constexpr size_t FRAMES_NONE = 0;
    static constexpr size_t REELS_WANT = 2;
    static constexpr size_t REELS_NONE = 0;

  public:
    size_t dst_size = 0;
    Nanos at_ns{0};
    Nanos elapsed_ns{0};
    size_t frames = 0;

    static constexpr csv moduleId{"REQUISITION"};

    Requisition(strand &src_strand, Reels &src, strand &dst_strand, Reels &dst)
        : src_strand(src_strand), src(src), dst_strand(dst_strand), dst(dst){};

    MillisFP elapsed() const { return pe_time::elapsed_as<MillisFP>(at_ns); }
    void finish(Reels &reels, shReel reel) { finish(reels.emplace_back(reel)->size()); }
    void finish(size_t reel_frames = 0) {
      elapsed_ns = pe_time::elapsed_abs_ns(at_ns);

      if (reel_frames) {
        __LOG0(LCOL01 " frames={} elapsed={:<0.3}\n", moduleId,
               reel_frames ? csv("FINISHED") : csv("INCOMPLETE"), reel_frames,
               pe_time::as_millis_fp(elapsed_ns));
      }

      at_ns = Nanos::zero();
      frames = 0;
    }

    void ifNeeded() {
      if (needReel()) {
        at_ns = pe_time::nowNanos(); // start requisition, guard by caller strand

        asio::post(src_strand, [this]() {
          if (src.empty()) { // src is empty, mark requisition as finished
            asio::post(dst_strand, [this]() { finish(); }); // via dst_strand
            return;
          }

          // a src Reel exists, are there enough frames?
          // must get a reference to the front Reel so it can be reset, if needed
          if (shReel &reel = src.front(); reel->size() >= frames) {
            // a shReel is present, finish via dst_strand
            asio::post(dst_strand,
                       [this, reel = reel->shared_from_this()]() { finish(dst, reel); });

            reel.reset(); // reset the shared_ptr from the src container
            std::erase_if(src, [](shReel reel) { return reel.use_count() == 0; });

          } else {
            asio::post(dst_strand, [this]() { finish(); }); // via dst_strand
          }
        });
      }
    }

    bool inProgress() const { return at_ns > Nanos::zero(); }
    bool needReel() {
      size_t dst_size = dst.size();

      if (inProgress() || (dst_size >= REELS_WANT)) {
        // req not needed: in progress or have the Reels required
        frames = FRAMES_NONE;
      } else if (dst_size == REELS_NONE) {
        // dst is empty: we need at least one Reel containing
        // a minimum number of Frames to begin output.
        // to feed caller (Render). this is common at start up or
        // after a flush.
        frames = FRAMES_MIN;
      } else if (dst_size < REELS_WANT) {
        // dst wants to cache one or more shReel to ensure a Reel is
        // available when the current shReel is exhausted
        frames = FRAMES_MAX;
      }

      return frames != FRAMES_NONE;
    }

  private:
    // order dependent
    strand &src_strand;
    Reels &src;

    strand &dst_strand;
    Reels &dst;
  };

private:
  Spooler(io_context &io_ctx)
      : module_id(csv("SPOOLER")), // set the module id
        strand_in(io_ctx),         // guard Reels reels_in
        strand_out(io_ctx),        // guard Reels reels_out
        requisition(strand_in, reels_in, strand_out, reels_out) {}

public:
  static shSpooler create(io_context &io_ctx) { return shSpooler(new Spooler(io_ctx)); }

public:
  void flush(const FlushRequest &flush);
  void loadTimeout();
  shFrame nextFrame(const Nanos &lead_time);
  shFrame queueFrame(shFrame frame);
  strand &strandOut() { return strand_out; }

  // misc stats and debug
  const string inspect() const;
  void logAsync() {
    asio::post(strand_in, [self = shared_from_this()]() { self->logSync(); });
  }
  void logSync() { __LOG0("{:<18} {}\n", module_id, inspect()); }
  const string &moduleId() const { return module_id; }

private:
  void flushReels(const FlushRequest &request, Reels &reels) {
    // create new reels containing the desired seq_nums
    Reels reels_keep;

    // transfer un-flushed rtp_packets to new reels
    ranges::for_each(reels, [&](shReel reel) {
      if (auto keep = reel->flush(request); keep == true) { // reel has frames
        __LOG0("{:<18} FLUSH KEEP {}\n", reel->moduleId(), Reel::inspect(reel));

        reels_keep.emplace_back(reel);
      }
    });

    // swap kept reels
    std::swap(reels_keep, reels);
  }

private:
  // order dependent (constructor initialized)
  string module_id;
  strand strand_in;
  strand strand_out;
  Reels reels_in;
  Reels reels_out;
  Requisition requisition; // guard by strand_out

  // order independent
  FlushRequest flush_request; // flush request (applied to reels_in and reels_out)
};

} // namespace player
} // namespace pierre