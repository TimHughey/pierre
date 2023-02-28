
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
      handoff_strand(io_ctx),                  // unprocessed frame 'queue'
      wip_strand(io_ctx),                      // guard wip reel
      flush_strand(io_ctx),                    // isolate flush (and interrupt other strands)
      wip_timer(io_ctx),                       // rack incomplete wip reels
      master_clock(master_clock),              // inject master clock dependency
      anchor(anchor) {
  INFO_INIT("sizeof={:>5} frame_sizeof={} lead_time={} fps={} thread_count={}\n", sizeof(Racked),
            sizeof(Frame), pet::humanize(InputInfo::lead_time), InputInfo::fps, thread_count);

  // initialize supporting objects
  av = std::make_unique<Av>(io_ctx);

  auto latch = std::make_unique<std::latch>(thread_count);
  shutdown_latch.emplace(thread_count);

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
  INFO_SHUTDOWN_REQUESTED();

  guard.reset();

  [[maybe_unused]] error_code ec;
  wip_timer.cancel(ec);

  shutdown_latch->wait();

  av.reset();

  INFO_SHUTDOWN_COMPLETE();
}

void Racked::emplace_frame(frame_t frame) noexcept {

  std::shared_lock<std::shared_timed_mutex> lck(flush_mtx, std::defer_lock);
  lck.lock();

  // create the wip, if needed
  if (wip.has_value() == false) wip.emplace();

  // all frames in a reel must be in ascending order for proper flushing
  if ((!wip->empty() && (*wip < frame)) || wip->empty()) {
    wip->add(frame);
  } else {
    // not in ascending order, rack the current wip and begin a new one
    rack_wip();

    wip.emplace().add(frame);
  }

  if (wip->full()) {
    rack_wip();
  } else if (std::ssize(*wip) == 1) {
    monitor_wip(); // handle incomplete wip reels
  }
}

void Racked::flush(FlushInfo &&request) noexcept {

  // post the flush request to the flush strand to serialize flush requests
  asio::post(flush_strand, [this, request = std::move(request)]() mutable {
    // record the flush request to check incoming frames (guarded by flush_strand)
    flush_request = std::move(request);

    flush_impl();
  });
}

void Racked::flush_impl() noexcept {
  static constexpr csv fn_id{"flush"};

  INFO_AUTO("{}\n", flush_request);

  std::unique_lock lck(flush_mtx, std::defer_lock);
  lck.lock();

  // rack wip so it is handled while processing racked
  rack_wip(LOCK_FREE);

  // now, to optimize the flush, let's first check if everything racked
  // is within the request.  if so, we can simply clear all racked reels
  int64_t reel_count = std::ssize(racked);
  int64_t flush_count = 0;

  if (reel_count > 0) {

    auto first = racked.begin()->peek_next();
    auto last = racked.rbegin()->peek_last();

    INFO_AUTO("rack contains seq_num {:<8}/{:>8} before flush\n", first->seq_num, last->seq_num);

    if (flush_request.matches<frame_t>({first, last})) {
      // yes, the entire rack matches the flush request so we can
      // take a short-cut and clear the entire rack in oneshot
      INFO_AUTO("clearing all reels={}\n", reel_count);

      racked_reels cleared;
      std::swap(racked, cleared);
      flush_count = reel_count;

    } else {
      // the entire rack did not match the flush request so now
      // we must use maximum effort and look at each reel

      std::erase_if(racked, [&flush_count, this](auto &reel) mutable {
        auto rc = reel.flush(flush_request);

        if (rc) flush_count++;

        return rc;
      });
    }

    const auto reels_now = std::ssize(racked);

    lck.unlock(); // unlock, container actions complete

    Stats::write(stats::REELS_FLUSHED, flush_count);
    Stats::write(stats::RACKED_REELS, reels_now);
  }
}

void Racked::handoff(uint8v &&packet, const uint8v &key) noexcept {
  if (!ready) return;         // quietly ignore packets when Racked is not ready
  if (packet.empty()) return; // quietly ignore empty packets

  auto frame = Frame::create(packet); // create frame, will also be moved in lambda (as needed)

  if (frame->state.header_parsed()) {
    if (flush_request.should_flush(frame)) {
      frame->flushed();
    } else if (frame->decipher(std::move(packet), key)) {
      // notes:
      //  1. we post to handoff_strand to control the concurrency of decoding
      //  2. we move the packet since handoff() is done with it
      //  3. we move the frame since no code beyond this point needs it

      asio::post(handoff_strand, [this, frame = std::move(frame)]() {
        // here we do the decoding of the audio data, if decode succeeds we
        // rack the frame into wip

        if (frame->decode(av.get())) [[likely]] {
          // decode success, rack the frame

          // we post to wip_strand to guard wip reel.  that said, we still use a mutex
          // because of flushing and so next_frame can temporarily interrupt inbound
          // decoded packet spooling.  this is important because once a wip is full it
          // must be spooled into racked reels which is also being read from for next_frame()
          asio::post(wip_strand, [this, frame = std::move(frame)]() mutable {
            emplace_frame(std::move(frame));
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

void Racked::monitor_wip() noexcept {
  static constexpr csv fn_id{"monitor_wip"};

  wip_timer.expires_from_now(10s);

  wip_timer.async_wait( // must run on wip_strand to protect containers
      asio::bind_executor(wip_strand, [this, serial_num = wip->serial_num()](const error_code ec) {
        if (ec) return; // wip timer error, bail out

        // note: serial_num is captured to compare to active wip serial number to
        //       prevent duplicate racking

        const auto serial_num = wip.has_value() ? wip->serial_num() : 0;

        if (wip.has_value() && (wip->serial_num() == serial_num)) {
          string msg;

          if (Logger::should_log(module_id, fn_id)) msg = fmt::format("{}", *wip);

          rack_wip();

          INFO_AUTO("INCOMPLETE {}\n", msg);
        } else {
          INFO_AUTO("PREVIOUSLY RACKED serial_num={}\n", serial_num);
        }
      }));
}

void Racked::log_racked(log_racked_rc rc) const noexcept {
  static constexpr csv fn_id{"log_racked"};

  if (Logger::should_log(module_id, fn_id)) {
    string msg;
    auto w = std::back_inserter(msg);

    auto reel_count = std::ssize(racked);

    switch (rc) {
    case log_racked_rc::NONE: {
      if (reel_count == 0) {
        auto frame_count = racked.cbegin()->size();
        if (frame_count == 1) fmt::format_to(w, "LAST FRAME");
      } else if ((reel_count >= 400) && ((reel_count % 10) == 0)) {
        fmt::format_to(w, "HIGH REELS reels={:<3}", reel_count);
      }
    } break;

    case log_racked_rc::RACKED: {
      if (reel_count == 1) {
        fmt::format_to(w, "FIRST REEL reels={:<3}", reel_count);
      }

      if (wip.has_value() && !wip->empty()) {
        fmt::format_to(w, " wip_reel={}", wip.value());
      }
    } break;

    default:
      break;
    }

    if (!msg.empty()) INFO_AUTO("{}\n", msg);
  }
}

frame_future Racked::next_frame(const Nanos max_wait) noexcept { // static
  auto prom = frame_promise();
  auto fut = prom.get_future().share();

  // when Racked isn't ready (doesn't exist) return SilentFrame
  if (ready.load() == false) {
    prom.set_value(SilentFrame::create());
    return fut;
  }

  asio::post(io_ctx, [=, this, prom = std::move(prom)]() mutable { //
    next_frame_impl(std::move(prom), max_wait);
  });

  // return the future
  return fut;
}

void Racked::next_frame_impl(frame_promise prom, const Nanos max_wait) noexcept {
  const Nanos wait_half(max_wait / 2);

  std::shared_lock<std::shared_timed_mutex> flush_lck(flush_mtx, std::defer_lock);
  std::unique_lock rack_lck{rack_mtx, std::defer_lock}; // don't lock yet

  auto master_clock_fut = master_clock->info();
  auto clock_info = ClockInfo();

  if (master_clock_fut.valid()) {
    if (master_clock_fut.wait_for(wait_half) == std::future_status::ready) {
      clock_info = master_clock_fut.get();
    }
  }

  if (clock_info.ok() == false) {
    // no clock info, set promise to SilentFrame
    prom.set_value(SilentFrame::create());
    return;
  } else [[likely]] {
    // we have ClockInfo, get AnchorLast
    clock_info = master_clock->info().get();
    auto anchor_now = anchor->get_data(clock_info);

    if ((anchor_now.ready() == false) || !spool_frames) {
      // anchor not ready yet or we haven't been instructed to spool
      // so set the promise to a SilentFrame. (i.e. user hit pause, hasn't hit
      // play or disconnected)
      prom.set_value(SilentFrame::create());
      return;
    }

    if (!flush_lck.try_lock()) { // we're flushing
      prom.set_value(SilentFrame::create());
      return;
    }

    if (!rack_lck.try_lock_for(wait_half)) { // racked contention
      prom.set_value(SilentFrame::create());
      return;
    }

    // do we have any racked reels? if not, return a SilentFrame
    if (std::empty(racked)) {
      prom.set_value(SilentFrame::create());
      return;
    }

    auto &reel = *racked.begin();
    auto frame = reel.peek_next();

    // calc the frame state (Frame caches the anchor)
    auto state = frame->state_now(anchor_now, InputInfo::lead_time);
    prom.set_value(frame);

    if (state.ready() || state.outdated() || state.future()) {
      // consume the ready or outdated frame
      if (auto reel_empty = reel.consume(); reel_empty == true) {
        // reel is empty after the consume, erase it
        racked.erase(racked.begin());
        Stats::write(stats::RACKED_REELS, std::ssize(racked));
      }

      log_racked();
    }
  }
}

void Racked::rack_wip(use_lock_t use_lock) noexcept {
  [[maybe_unused]] error_code ec;
  wip_timer.cancel(ec); // wip reel is full, cancel the incomplete spool timer

  log_racked_rc rc{NONE};

  std::unique_lock lck{rack_mtx, std::defer_lock};
  if (wip && !wip->empty()) {

    if (use_lock == LOCK) lck.lock();

    racked.emplace_back(std::move(wip.value()));
    wip.reset();
    rc = RACKED;
  }

  if (use_lock == LOCK) lck.unlock();

  if (rc == RACKED) Stats::write(stats::RACKED_REELS, std::ssize(racked));

  log_racked(rc);
}

} // namespace pierre