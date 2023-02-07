
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
#include "io/io.hpp"
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

// static class data
int64_t Racked::REEL_SERIAL_NUM{0x1000};

Racked::Racked(MasterClock *master_clock) noexcept
    : thread_count(config_threads<Racked>(3)), // thread count
      guard(asio::make_work_guard(io_ctx)),    // ensure io_ctx has work
      handoff_strand(io_ctx),                  // unprocessed frame 'queue'
      wip_strand(io_ctx),                      // guard work in progress reeel
      frame_strand(io_ctx),                    // used for next frame
      flush_strand(io_ctx),                    // isolate flush (and interrupt other strands)
      wip_timer(io_ctx),                       // rack incomplete wip reels
      master_clock(master_clock)               // inject master clock dependency
{
  INFO_INIT("sizeof={:>5} frame_sizeof={} lead_time={} fps={} thread_count={}\n", sizeof(Racked),
            sizeof(Frame), pet::humanize(InputInfo::lead_time), InputInfo::fps, thread_count);

  // initialize supporting objects
  Anchor::init();
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

  latch->wait(); // caller waits until all threads are started

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

void Racked::flush(FlushInfo &&request) {
  static constexpr csv fn_id{"flush"};

  asio::post(flush_strand, [this, request = std::move(request)]() mutable {
    INFO_AUTO("{}\n", request);

    // record the flush request to check incoming frames (guarded by flush_strand)
    flush_request = std::move(request);

    // flush wip first so we can unlock it quickly
    if (wip.has_value()) {
      // lock both the wip and rack
      std::unique_lock lck_wip(wip_mtx, std::defer_lock);
      lck_wip.lock();

      // if wip is completely flushed reset the optional
      if (wip->flush(flush_request)) {
        [[maybe_unused]] error_code ec;
        wip_timer.cancel(ec);
        wip.reset();
      }
    }

    std::unique_lock lck_rack{rack_mtx, std::defer_lock};
    lck_rack.lock();

    // now, to optimize the flush, let's first check if everything racked
    // is within the request.  if so, we can simply clear all racked reels
    auto initial_size = std::ssize(racked);
    if (initial_size > 0) {

      auto first = racked.begin()->second.peek_first();
      auto last = racked.rbegin()->second.peek_last();

      INFO_AUTO("rack contains seq_num {:<8}/{:>8} before flush\n", first->seq_num, last->seq_num);

      if (flush_request.matches<frame_t>({first, last})) {
        // yes, the entire rack matches the flush request so we can
        // take a short-cut and clear the entire rack in oneshot
        INFO_AUTO("clearing all reels={}\n", initial_size);

        racked_reels cleared;
        std::swap(racked, cleared);

      } else {
        // the entire rack did not match the flush request so now
        // we must use maximum effort and look at each reel

        for (auto it = racked.begin(); it != racked.end();) {
          auto &reel = it->second;

          if (reel.flush(flush_request)) { // true=reel empty
            it = racked.erase(it);         // returns it to next not erased entry
            Stats::write(stats::RACKED_REELS, std::ssize(racked));
          } else {
            INFO_AUTO("KEEPING {}\n", reel);
          }
        }
      }

      const auto flushed = initial_size - std::ssize(racked);
      Stats::write(stats::REELS_FLUSHED, int64_t{flushed > 0 ? flushed : initial_size});
    }

    // has everything been flushed?
    if (racked.empty() && !wip) {
      first_frame.reset(); // signal we're starting fresh
    }
  });
}

void Racked::handoff(uint8v &&packet, const uint8v &key) noexcept {
  if (ready.load() == false) return; // quietly ignore packets when Racked is not ready
  if (packet.empty()) return;        // quietly ignore empty packets

  auto frame = Frame::create(packet); // create frame, will also be moved in lambda (as needed)

  if (frame->state.header_parsed()) {
    if (flush_request.should_flush(frame)) {
      frame->flushed();
    } else if (frame->decipher(std::move(packet), key)) {
      // notes:
      //  1. we post to handoff_strand to control the concurrency of decoding
      //  2. we move the packet since handoff() is done with it
      //  3. we grab a reference to handoff strand since we move s
      //  4. we move the frame since no code beyond this point needs it

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
            std::unique_lock lck(wip_mtx, std::defer_lock);
            lck.lock();

            // create the wip, if needed
            if (wip.has_value() == false) wip.emplace(++REEL_SERIAL_NUM);

            wip->add(frame);

            if (wip->full()) {
              rack_wip();
            } else if (std::ssize(*wip) == 1) {
              monitor_wip(); // handle incomplete wip reels
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

void Racked::monitor_wip() noexcept {
  static constexpr csv fn_id{"monitor_wip"};

  wip_timer.expires_from_now(10s);

  wip_timer.async_wait( // must run on wip_strand to protect containers
      asio::bind_executor(wip_strand, [this, serial_num = wip->serial_num()](const error_code ec) {
        // note: serial_num is captured to ensure the same wip reel is active when
        //       the timer fires

        if (!ec && !racked.contains(serial_num)) {
          INFOX(module_id, fn_id, "INCOMPLETE {}\n", wip->inspect());
          Stats::write(stats::RACK_WIP_INCOMPLETE, wip->size());
          // not a timer error and reel is the same
          rack_wip();
        } else if (!ec) {
          INFO_AUTO("reel serial_num={} already racked\n", serial_num);
        }
      }));
}

frame_future Racked::next_frame() noexcept { // static
  auto prom = frame_promise();
  auto fut = prom.get_future().share();

  // when Racked isn't ready (doesn't exist) return SilentFrame
  if (ready.load() == false) {
    prom.set_value(SilentFrame::create());
    return fut;
  }

  asio::post(frame_strand, [this, prom = std::move(prom)]() mutable {
    std::unique_lock lck{rack_mtx, std::defer_lock}; // don't lock yet

    // do we have any racked reels? if not, return a SilentFrame
    lck.lock(); // guard std::empty(racked) call
    if (std::empty(racked)) {
      prom.set_value(SilentFrame::create());
      return;
    }

    // we have racked reels but need clock_info, unlock while we wait
    lck.unlock();

    auto master_clock_fut = master_clock->info();
    auto clock_info = ClockInfo();

    if (master_clock_fut.valid()) {
      auto master_clock_result = master_clock_fut.wait_for(InputInfo::lead_time_min);
      if (master_clock_result == std::future_status::ready) {
        clock_info = master_clock_fut.get();
      }
    }

    if (clock_info.ok() == false) {
      // no clock info, set promise to SilentFrame
      prom.set_value(SilentFrame::create());
    } else [[likely]] {
      // we have ClockInfo and there are racked reels
      // refresh clock info and get anchor info
      clock_info = master_clock->info().get();
      auto anchor = Anchor::get_data(clock_info);

      // anchor not ready yet or we haven't been instructed to spool
      // so set the promise to a SilentFrame. (i.e. user hit pause, hasn't hit
      // play yet or disconnected)
      //
      // note:  we could have done this check very early and completely avoid
      // the asio::post but didn't so we could prime ClockInfo and Anchor.
      // plus the above code isn't really that expensive every ~23ms.
      if ((anchor.ready() == false) || !spool_frames) {
        prom.set_value(SilentFrame::create());
        return;
      }

      lck.lock(); // lock since since we're looking at racked reels

      auto &reel = racked.begin()->second; // avoid multiple begin()->second
      auto frame = reel.peek_first();

      // calc the frame state (Frame caches the anchor)
      auto state = frame->state_now(anchor, InputInfo::lead_time);
      prom.set_value(frame);

      if (state.ready() || state.outdated() || state.future()) {
        // consume the ready or outdated frame
        reel.consume();

        // if the reel is empty, discard it
        if (reel.empty()) {
          racked.erase(racked.begin());
          Stats::write(stats::RACKED_REELS, std::ssize(racked));
        }

        log_racked();
      }
    }
  });

  // return the future
  return fut;
}

void Racked::rack_wip() noexcept {
  [[maybe_unused]] error_code ec;
  wip_timer.cancel(ec); // wip reel is full, cancel the incomplete spool timer

  log_racked_rc rc{NONE};

  std::unique_lock lck{rack_mtx, std::defer_lock};
  if (!wip->empty()) {

    if (lck.try_lock_for(InputInfo::lead_time)) [[likely]] {

      // get a copy of serial_num since emplace moving can be unpredictable
      const auto serial_num = wip->serial_num();

      auto it = racked.try_emplace( //
          racked.cend(),            // hint: at end
          serial_num,               // key: serial num,
          std::move(wip.value()));

      rc = it->first == serial_num ? RACKED : COLLISION;

    } else {
      rc = TIMEOUT;
    }

    if (rc == COLLISION) {
      Stats::write(stats::RACK_COLLISION, true);
    } else if (rc == RACKED) {
      Stats::write(stats::RACKED_REELS, std::ssize(racked));
    } else if (rc == TIMEOUT) {
      Stats::write(stats::RACK_WIP_TIMEOUT, true);
    }

    log_racked(rc);

    // regardless of the outcome above, reset wip
    wip.reset();
  }
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
        auto frame_count = racked.cbegin()->second.size();
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

} // namespace pierre