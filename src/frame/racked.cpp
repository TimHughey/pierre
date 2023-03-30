
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
#include "base/thread_util.hpp"
#include "base/types.hpp"
#include "frame.hpp"
#include "frame/flush_info.hpp"
#include "io/post.hpp"
#include "io/timer.hpp"
#include "lcs/config.hpp"
#include "lcs/stats.hpp"
#include "master_clock.hpp"
#include "silent_frame.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <boost/asio/append.hpp>
#include <latch>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <stop_token>
#include <vector>

namespace pierre {

Racked::Racked(MasterClock *master_clock, Anchor *anchor) noexcept
    : thread_count(config_threads<Racked>(3)), // thread count
      guard(asio::make_work_guard(io_ctx)),    // ensure io_ctx has work
      flush_strand(io_ctx),                    // ensure flush requests are serialized
      handoff_strand(io_ctx),                  // unprocessed frame 'queue'
      wip_strand(io_ctx),                      // guard wip reel
      racked_strand(io_ctx),                   // guard racked
      wip_timer(io_ctx),                       // rack incomplete wip reels
      master_clock(master_clock),              // inject master clock dependency
      anchor(anchor) {
  INFO_INIT("sizeof={:>5} frame_sizeof={} lead_time={} fps={} thread_count={}\n", sizeof(Racked),
            sizeof(Frame), pet::humanize(InputInfo::lead_time), InputInfo::fps, thread_count);

  // initialize supporting objects
  av = std::make_unique<Av>(io_ctx);

  auto latch = std::make_shared<std::latch>(thread_count);
  shutdown_latch = std::make_shared<std::latch>(thread_count);

  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n = n, latch = latch.get()]() {
      const auto thread_name = thread_util::set_name("racked", n);
      latch->count_down();

      INFO_THREAD_START();
      io_ctx.run();

      shutdown_latch->count_down();
      INFO_THREAD_STOP();
    }).detach();
  }

  latch->wait(); // caller waits until all workers are started

  ready = true;
}

Racked::~Racked() noexcept {
  ready = false;

  INFO_SHUTDOWN_REQUESTED();

  io_ctx.stop();

  shutdown_latch->wait();

  av.reset();

  INFO_SHUTDOWN_COMPLETE();
}

void Racked::flush(FlushInfo &&request) noexcept {
  static constexpr csv fn_id{"flush"};

  // post the flush request to the flush strand to serialize flush requests
  // and guard changes to flush request
  asio::post(flush_strand, [this, request = std::move(request)]() mutable {
    // record the flush request to check incoming frames (guarded by flush_strand)
    std::exchange(flush_request, std::move(request));

    asio::post(handoff_strand, [this]() { INFO_AUTO("{}\n", flush_request); });

    // rack wip so it is checked during rack flush
    rack_wip();

    // start the flush on the racked strand to prevent data races
    asio::post(racked_strand, [this]() {
      const auto reel_count = std::ssize(racked);
      int64_t flush_count{0};

      if (reel_count > 0) {

        // examine each reel to determine if it should be flushed
        // note:  this is necessary because frame timestamps can be out of sequence
        //        depending on the sensder's choice
        std::erase_if(racked, [this, &flush_count](auto &reel) mutable {
          const auto rc = reel.flush(flush_request);

          if (rc) {
            flush_count++;

            asio::post(handoff_strand, [serial_num = reel.serial_num()]() {
              INFO_AUTO("REEL 0x{:x} flushed\n", serial_num);
            });
          }

          return rc;
        });

        // we use the handoff_strand for the dirty business of logging and stats because it
        // is OK if inbound frame processing slows down a bit

        asio::post(handoff_strand, [=, reels_now = std::ssize(racked)]() {
          INFO_AUTO("reel_count={} flush_count={} reels_remaining={}\n", reel_count, flush_count,
                    reels_now);

          Stats::write(stats::REELS_FLUSHED, flush_count);
          Stats::write(stats::RACKED_REELS, reels_now);
        });
      }
    });
  });
}

void Racked::handoff(uint8v &&packet, const uint8v &key) noexcept {
  static constexpr auto fn_id{"handoff"};

  if (!ready) return;         // quietly ignore packets when Racked is not ready
  if (packet.empty()) return; // quietly ignore empty packets

  auto frame = Frame::create(packet); // create frame, will also be moved in lambda (as needed)

  if (frame->state.header_parsed()) {
    if (frame->decipher(std::move(packet), key)) {
      // notes:
      //  1. we post to handoff_strand to control the concurrency of decoding
      //  2. we move the packet since handoff() is done with it

      asio::post(handoff_strand, [this, frame = std::move(frame)]() {
        // here we do the decoding of the audio data, if decode succeeds we
        // rack the frame into wip

        if (frame->decode(av.get())) [[likely]] {
          // decode in progress, proceed with adding to the wip via wip_strand

          asio::post(wip_strand, [this, frame = std::move(frame)]() mutable {
            if (frame->state.dsp_any() == false) {
              // ensure the frame is in a DSP state, if not then something is wrong and
              // the frame should be dropped.
              //
              // note:  dsp failures are detected when the frame is selected as the next frame

              INFO_AUTO("attempt to add wip frame not in a DSP state, dropping\n");
              return;
            }

            // prevent flushed frames from being added

            ////
            //// TODO:  may need to protect against data races...
            ////
            if (flush_request.should_flush(frame)) {
              frame->flushed();

            } else if (wip.add(frame) == false) {
              // the reel is full or the frame timestamp is not as expected, rack the current
              // wip reel (which automatically creates a new reel) then add the frame
              // note:  calls to add() when the reel is empty will never fail
              rack_wip();

              wip.add(frame);

              if (std::ssize(wip) == 1) monitor_wip(); // handle incomplete wip reels
            }
          });

        } else {
          frame->state.record_state(); // save the failue to timeseries database
        }
      });
    }
  } else {
    frame->state.record_state();
  }
}

void Racked::log_racked(log_rc rc) const noexcept {
  static csv constexpr fn_id{"log"};

  if (Logger::should_log(module_id, fn_id)) {
    const auto reel_count = std::ssize(racked);
    const auto reel_size = std::ssize(racked.front());
    const auto wip_size = std::ssize(wip);

    string msg;
    auto w = std::back_inserter(msg);

    switch (rc) {
    case log_rc::NONE: {
      if ((reel_count == 1) && (reel_size == 1)) {

        fmt::format_to(w, "LAST FRAME");
      } else if ((reel_count >= 400) && ((reel_count % 10) == 0)) {

        fmt::format_to(w, "HIGH REELS reels={:<3}", reel_count);
      }
    } break;

    case log_rc::RACKED: {
      if (reel_count == 1) fmt::format_to(w, "FIRST REEL frames={}", reel_size);
      if (wip_size) fmt::format_to(w, " wip_size={}", wip_size);

    } break;
    }

    if (msg.size()) INFO_AUTO("{}\n", msg);
  }
}

void Racked::monitor_wip() noexcept {
  static csv constexpr fn_id{"monitor_wip"};

  wip_timer.expires_from_now(10s);

  wip_timer.async_wait( // must run on wip_strand to protect containers
      asio::bind_executor(wip_strand, [this, serial_num = wip.serial_num()](const error_code ec) {
        if (ec) return; // wip timer error (liekly operation cancelled), bail out

        // note: serial_num is captured to compare to active wip serial number to
        //       prevent duplicate racking

        if (wip.serial_num() == serial_num) {
          string msg;

          if (Logger::should_log(module_id, fn_id)) msg = fmt::format("{}", wip);

          rack_wip();

          INFO_AUTO("INCOMPLETE {}\n", msg);
        } else {
          INFO_AUTO("PREVIOUSLY RACKED serial_num={}\n", serial_num);
        }
      }));
}

frame_t Racked::next_frame_no_wait() noexcept {
  static csv constexpr fn_id{"next_frame"};

  if (!ready.load() || !spool_frames.load() || std::empty(racked)) return SilentFrame::create();

  const auto clock_info = master_clock->info_no_wait();
  if (clock_info.ok() == false) return SilentFrame::create();

  auto anchor_now = anchor->get_data(clock_info);
  if (anchor_now.ready() == false) return SilentFrame::create();

  auto &reel = racked.front();
  auto frame = reel.peek_next();

  // calc the frame state (Frame caches the anchor)
  const auto state = frame->state_now(anchor_now, InputInfo::lead_time);

  if (state.ready() || state.outdated() || state.future()) {
    // consume the ready or outdated frame
    if (auto reel_empty = reel.consume(); reel_empty == true) {
      // reel is empty after the consume, pop it
      // asio::post(racked_strand, [this]() { racked.pop_front(); });

      racked.pop_front();
    }

    asio::post(handoff_strand, [=, this, racked_size = std::ssize(racked)]() {
      log_racked();
      Stats::write(stats::RACKED_REELS, racked_size);
    });

  } else {
    asio::post(handoff_strand, [=]() { INFO_AUTO("frame state={} present in racked\n", state); });
  }

  return frame;
}

void Racked::rack_wip() noexcept {
  [[maybe_unused]] error_code ec;
  wip_timer.cancel(ec); // wip reel is full, cancel the incomplete spool timer

  auto rc{NONE};

  if (wip.empty() == false) {
    // capture the current wip reel (while creating a new one) then use racked_strand
    // to add it to racked reels
    asio::post(racked_strand,
               [this, reel = std::move(wip)]() mutable { racked.emplace_back(std::move(reel)); });

    rc = RACKED;

    wip = Reel();
  }

  if (rc == RACKED) Stats::write(stats::RACKED_REELS, std::ssize(racked));

  log_racked(rc);
}

} // namespace pierre