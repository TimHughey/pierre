
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
#include "base/types.hpp"
#include "frame.hpp"
#include "frame/flush_info.hpp"
#include "lcs/config.hpp"
#include "lcs/stats.hpp"
#include "master_clock.hpp"
#include "silent_frame.hpp"

#include <algorithm>
#include <atomic>
#include <boost/asio/append.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace pierre {

Racked::Racked() noexcept
    : thread_count(config_threads<Racked>(3)),               // thread count
      thread_pool(thread_count),                             //
      flush_strand(asio::make_strand(thread_pool)),          // ensure flush requests are serialized
      handoff_strand(asio::make_strand(thread_pool)),        // unprocessed frame 'queue'
      racked_strand(asio::make_strand(thread_pool)),         // guard racked
      wip(std::make_unique<Reel>(Reel::DEFAULT_MAX_FRAMES)), //
      av(std::make_unique<Av>())                             //
{
  INFO_INIT("sizeof={:>5} frame_sizeof={} lead_time={} fps={} thread_count={}\n", sizeof(Racked),
            sizeof(Frame), pet::humanize(InputInfo::lead_time), InputInfo::fps, thread_count);
}

Racked::~Racked() noexcept {
  thread_pool.stop();
  thread_pool.join();

  av.reset();
}

void Racked::flush(FlushInfo &&request) noexcept {
  static constexpr csv fn_id{"flush"};

  // post the flush request to the flush strand to serialize flush requests
  // and guard changes to flush request
  asio::post(flush_strand, [this, request = std::move(request)]() mutable {
    // record the flush request to check incoming frames (guarded by flush_strand)
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
          const auto rc = reel->flush(flush_request);

          if (rc) {
            flush_count++;

            INFO_AUTO("FLUSHED {}\n", *reel);
          }

          return rc;
        });

        // we use the handoff_strand for the dirty business of logging and stats because it
        // is OK if inbound frame processing slows down a bit

        asio::post(handoff_strand, [=, reels_now = reel_count()]() {
          INFO_AUTO("flush_count={} reels_remaining={}\n", flush_count, reels_now);

          Stats::write(stats::REELS_FLUSHED, flush_count);
          Stats::write(stats::RACKED_REELS, initial_reels);
        });
      }
    });
  });
}

void Racked::handoff(uint8v &&packet, const uint8v &key) noexcept {
  static constexpr auto fn_id{"handoff"};

  if (packet.empty()) return; // quietly ignore empty packets

  auto frame = Frame::create(packet); // create frame, will also be moved in lambda (as needed)

  if (frame->state.header_parsed()) {
    if (frame->decipher(std::move(packet), key)) {
      // notes:
      //  1. we post to handoff_strand to control the concurrency of decoding
      //  2. we move the packet since handoff() is done with it

      if (flush_request.should_flush(frame)) {
        // frame is flushed, record it's state and don't move to wip
        frame->flushed();
        frame->state.record_state();
      } else {
        asio::post(handoff_strand, [this, frame = std::move(frame)]() {
          // here we do the decoding of the audio data, if decode succeeds we
          // rack the frame into wip

          if (frame->decode(av.get())) [[likely]] {
            // decode in progress, proceed with adding to the wip via wip_strand
            frame->state.record_state();

            // prevent flushed frames from being added
            if (frame->state.dsp_any()) {

              // note:
              //  -reel::add() returns false when the reel is full or the timestamp
              //   of the frame is not ordered sequentially
              //  -when false is returned racked must create a new wip for the frame

              if (wip->add(std::move(frame)) == false) {

                // rack_wip() moves the full wip into reels then creates a new
                // wip and adds the frame to it
                rack_wip(std::move(frame));
              }

              // reset recent handoff which is used to determine if a wip should be
              // returned by next_frame().  this feature is to ensure incomplete reels
              // are returned by next_reel when required
              recent_handoff.reset();

            } else {
              // something wrong, the frame is not in a DSP state. discard...
              INFO_AUTO("discarding frame {}\n", frame->state);
            }
          }
        });
      }
    } else {
      frame->state.record_state();
    }
  } else {
    frame->state.record_state();
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
      if ((total_reels == 1) && (reels.front()->size() == 1)) {

        fmt::format_to(w, "LAST FRAME");
      } else if ((total_reels >= 400) && ((total_reels % 10) == 0)) {

        fmt::format_to(w, "HIGH REELS reels={:<3}", total_reels);
      }
    } break;

    case log_rc::RACKED: {
      if (total_reels == 1) {
        fmt::format_to(w, "FIRST REEL frames={}", reels.front()->size());
        if (wip->size() == 1) fmt::format_to(w, " [new wip]");
      }

    } break;
    }

    if (msg.size()) INFO_AUTO("{}\n", msg);
  }
}

std::future<std::unique_ptr<Reel>> Racked::next_reel() noexcept {

  auto prom = std::promise<std::unique_ptr<Reel>>();
  auto fut = prom.get_future();

  asio::post(racked_strand, [this, prom = std::move(prom)]() mutable {
    if (reel_count() > 0) {

      // set the promise as quickly as possible
      prom.set_value(std::move(reels.front()));

      asio::post(racked_strand, [this]() {
        reels.pop_front();

        Stats::write(stats::RACKED_REELS, reel_count());
      });

    } else if (recent_handoff >= 1500ms) {

      prom.set_value(std::move(take_wip()));
    } else {

      prom.set_value(std::make_unique<Reel>());
    }
  });

  return fut;
}

void Racked::rack_wip(frame_t frame) noexcept {
  if (wip->size() > 0) {

    asio::post(racked_strand, [=, this]() mutable {
      reels.emplace_back(std::move(take_wip()));

      if (frame) wip->add(frame);

      Stats::write(stats::RACKED_REELS, reel_count());

      log_racked(RACKED);
    });
  }
}

} // namespace pierre