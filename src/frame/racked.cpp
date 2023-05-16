
// Pierre - Custom Light Show for Wiss Landing
// Copyright (C) 2021  Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "racked.hpp"
#include "anchor.hpp"
#include "av.hpp"
#include "base/pet.hpp"
#include "base/stats.hpp"
#include "base/types.hpp"
#include "frame.hpp"
#include "frame/flush_info.hpp"
#include "master_clock.hpp"

#include <boost/asio/consign.hpp>
#include <boost/asio/prepend.hpp>
#include <fmt/std.h>

namespace pierre {

Racked::Racked() noexcept
    : // allocate our threads
      threads(nthreads),
      // Racked provides request based service so needs a work_guard
      work_guard(asio::make_work_guard(io_ctx)),
      // create a strand to guard access to frames and flush_request
      frames_strand(asio::make_strand(io_ctx)),
      // start with an inactive flush request
      flush_request(FlushInfo::Inactive),
      // as of C++20 constructor clears flag
      next_reel_busy(), reel_ready(),
      // initialize our first reel
      reel(Reel::Transfer) //
{
  INFO_INIT("sizeof={:>5} frame_sizeof={} lead_time={} fps={} nthreads={}", sizeof(Racked),
            sizeof(Frame), pet::humanize(InputInfo::lead_time), InputInfo::fps, nthreads);

  auto n_thr = nthreads;
  for (auto &t : threads) {
    const auto tname = fmt::format("pierre_rack{}", n_thr--);

    t = std::jthread([this, tname = std::move(tname)]() {
      pthread_setname_np(pthread_self(), tname.c_str());

      INFO("thread"sv, "started {}", tname);

      io_ctx.run();
    });
  }
}

Racked::~Racked() noexcept {
  INFO_AUTO_CAT("destruct");

  io_ctx.stop();

  INFO_AUTO("joining threads={} io_ctx={}", threads.size(), io_ctx.stopped());

  for (auto &t : threads) {
    if (t.joinable()) {
      INFO_AUTO("attempting to join tid={}", t.get_id());

      t.join();
    }
  }
}

void Racked::flush(FlushInfo &&fi) noexcept {
  INFO_AUTO_CAT("flush");

  asio::post(frames_strand, [this, fi = std::move(fi)]() mutable {
    // first, save FlushInfo
    flush_request = std::move(fi);

    if (auto flushed = reel.flush(flush_request); flushed > 0) {
      INFO_AUTO("flushed={} avail={}", flushed, reel.size_cached());
      if (reel.empty()) {
        reel = Reel(Reel::Transfer);
        ts_oos.clear();
      }
    }

    flush_request.done();

    // second, flush frames
    /*auto flush_n = flush_request(reel, frame::FLUSHED);

     auto [erased, remain] = reel.clean();

     INFO_AUTO("DONE flushed={} erased={} remain=[] ts_oos={}", flush_n, erased, remain,
               ts_oos.size());

     // create a new reel if the previous reel was emptied by the flush
     if (remain == 0) {
       reel = Reel(Reel::Transfer);
       ts_oos.clear();
     }

     flush_request.done();*/
  });
}

void Racked::handoff(uint8v &&packet, const uint8v &key) noexcept {
  INFO_AUTO_CAT("handoff");

  if (packet.empty()) return; // quietly ignore empty packets

  // wait for any in-progress flush requests to finish
  if (flush_request.busy_hold(frames_strand) == false) {
    INFO_AUTO("{}", flush_request.pop_msg(FlushInfo::BUSY_MSG));
  }

  // perform the heavy lifting of decipher, decode and dsp in the caller's thread
  Frame frame(packet);
  frame.process(std::move(packet), key);
  flush_request.check(frame, frame::FLUSHED);

  frame.record_state();

  INFO_AUTO("created {}", frame);

  if (frame.can_render()) reel.push_back(std::move(frame));

  /* asio::post(frames_strand, //
              asio::prepend(
                  [this](Frame frame) {
                    // when a deviation in the sender's timeline (timestamp) is
                    // encoutered record the timestamp of the previous frame
                    // for creating reels, flushing, etc.
                    if (reel.size() > 1) {
                      const auto &last = reel.back();

                      if (frame.ts() != (last.ts() + 1024)) {
                        auto [it, inserted] = ts_oos.insert(frame.ts());
                        if (!inserted) INFO_AUTO("ts_oos {} {}", *it, frame);
                      }
                    }

                    reel_p

                    xfer_reel(); // no-op if no reel xfer pending

                    // now add the incoming frame
                    std::lock_guard<decltype(reel_mtx)> reel_lock(reel_mtx);
                    reel.push_back(std::move(frame));
                  },
                  std::move(frame))); */
}

} // namespace pierre