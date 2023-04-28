
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

#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace pierre {

Racked::Racked() noexcept
    : // Racked provides request based service so needs a work_guard
      work_guard(asio::make_work_guard(io_ctx)),
      // ensure flush requests are serialized
      flush_strand(asio::make_strand(io_ctx)),
      // serialize changes to reels container
      racked_strand(asio::make_strand(io_ctx)),
      // create the initial work-in-progress reel
      wip(Reel(Reel::MaxFrames)) //
{

  // start the threads and run the io_ctx
  for (auto n = 0; n < nthreads; n++) {
    std::jthread([this, n = n]() {
      static constexpr csv tname{"pierre_racked"};

      pthread_setname_np(pthread_self(), tname.data());

      INFO(Racked::module_id, "threads", "startihg thread={}\n", n);

      io_ctx.run();
    }).detach();
  }

  INFO_INIT("sizeof={:>5} frame_sizeof={} lead_time={} fps={} nthreads={}\n", sizeof(Racked),
            sizeof(Frame), pet::humanize(InputInfo::lead_time), InputInfo::fps, nthreads);
}

Racked::~Racked() noexcept { io_ctx.stop(); }

void Racked::flush(FlushInfo &&request) noexcept {
  static constexpr csv fn_id{"flush"};

  // post the flush request to the flush strand to serialize flush requests
  // and guard changes to flush request
  asio::post(flush_strand, [this, request = std::move(request)]() mutable {
    // store the flush request to check incoming frames (guarded by flush_strand)
    std::exchange(flush_request, std::move(request));

    // rack wip so it is checked during rack flush
    rack_wip();

    // start the flush on the racked strand to prevent data races
    asio::post(racked_strand, [this]() {
      const auto initial_reels = reel_count();
      int64_t flush_count{0};

      if (initial_reels > 0) {

        // examine each reel to determine if it should be flushed
        // note:  this is necessary because frame timestamps can be out of sequence
        //        depending on the sensder's choice
        std::erase_if(reels, [this, &flush_count](auto &reel) mutable {
          const auto rc = reel.flush(flush_request);

          if (rc) {
            flush_count++;

            INFO_AUTO("FLUSHED {}\n", reel);
          }

          return rc;
        });

        INFO_AUTO("flush_count={} reels_remaining={}\n", flush_count, reel_count());

        Stats::write(stats::REELS_FLUSHED, flush_count);
        Stats::write(stats::RACKED_REELS, initial_reels);
      }
    });
  });
}

void Racked::handoff(uint8v &&packet, const uint8v &key) noexcept {
  static constexpr auto fn_id{"handoff"};

  if (packet.empty()) return; // quietly ignore empty packets

  Frame frame(packet); // create frame, will also be moved in lambda (as needed)

  if (frame.state.header_parsed()) {
    if (frame.decipher(std::move(packet), key)) {
      // notes:
      //  1. we post to handoff_strand to control the concurrency of decoding
      //  2. we move the packet since handoff() is done with it

      if (flush_request.should_flush(frame)) {
        // frame is flushed, record it's state and don't move to wip
        frame.flushed();
        frame.record_state();
      } else {

        // here we do the decoding of the audio data, if decode succeeds we
        // rack the frame into wip

        if (frame.decode()) [[likely]] {
          frame.record_state();

          // prevent flushed frames from being added
          if (frame.state.dsp_any()) {
            if (wip.can_add_frame(frame) == false) rack_wip();

            wip.add(std::move(frame));

          } else {
            // something wrong, the frame is not in a DSP state. discard...
            INFO_AUTO("discarding frame {}\n", frame.state);
          }
        }
      }
    } else {
      frame.record_state();
    }
  } else {
    frame.record_state();
  }
}

void Racked::log_racked(log_rc rc) const noexcept {
  static csv constexpr fn_id{"log"};

  if (Logger::should_log(module_id, fn_id)) {
    const auto total_reels = reel_count();

    string msg;
    auto w = std::back_inserter(msg);

    switch (rc) {
    case log_rc::NONE: {
      if ((total_reels == 1) && (reels.front().size() == 1)) {

        fmt::format_to(w, "LAST FRAME");
      } else if ((total_reels >= 400) && ((total_reels % 10) == 0)) {

        fmt::format_to(w, "HIGH REELS reels={:<3}", total_reels);
      }
    } break;

    case log_rc::RACKED: {
      if (total_reels == 1) {
        fmt::format_to(w, "FIRST REEL frames={}", reels.front().size());
        if (wip.size() == 1) fmt::format_to(w, " [new wip]");
      }

    } break;
    }

    if (msg.size()) INFO_AUTO("{}\n", msg);
  }
}

std::future<Reel> Racked::next_reel() noexcept {
  static constexpr auto fn_id{"next_reel"sv};

  auto prom = std::promise<Reel>();
  auto fut = prom.get_future();

  asio::post(racked_strand, [this, prom = std::move(prom)]() mutable {
    if (reel_count() > 0) {

      // set the promise as quickly as possible
      prom.set_value(std::move(reels.front()));

      reels.pop_front();

      Stats::write(stats::RACKED_REELS, reel_count());

    } else if (!wip.empty()) {
      INFO_AUTO("taking WIP {} reels={}\n", wip, reel_count());

      // we can take the wip reel
      prom.set_value(take_wip());
    } else {
      INFO_AUTO("no reels, returning synthetic reel\n");
      prom.set_value(Reel(Reel::Synthetic));
    }
  });

  return fut;
}

void Racked::rack_wip() noexcept {
  if (wip.size() > 0) {

    asio::post(racked_strand, [this, reel = take_wip()]() mutable {
      reels.emplace_back(std::move(reel));

      Stats::write(stats::RACKED_REELS, reel_count());

      log_racked(RACKED);
    });
  }
}

Reel Racked::take_wip() noexcept {
  std::unique_lock lck(wip_mtx, std::defer_lock);

  lck.lock();

  return std::exchange(wip, Reel(Reel::MaxFrames));
}

} // namespace pierre