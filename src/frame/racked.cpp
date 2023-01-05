
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
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"
#include "frame.hpp"
#include "frame/flush_info.hpp"
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
std::shared_ptr<Racked> Racked::self;

void Racked::flush(FlushInfo request) { // static
  static constexpr csv fn_id{"flush"};

  if (self.use_count() < 1) return;

  auto s = ptr();
  auto &flush_strand = s->flush_strand;

  asio::post(flush_strand, [s = std::move(s), request = std::move(request)]() mutable {
    INFO(module_id, fn_id, "{}\n", request);

    // save shared_ptr dereferences for multiple use variables
    auto &flush_request = s->flush_request;
    auto &racked = s->racked;
    auto &wip = s->wip;

    // record the flush request to check incoming frames (guarded by flush_strand)
    flush_request = std::move(request);

    // flush wip first so we can unlock it quickly
    if (wip.has_value()) {
      // lock both the wip and rack
      std::unique_lock lck_wip(s->wip_mtx, std::defer_lock);
      lck_wip.lock();

      // if wip is completely flushed reset the optional
      if (wip->flush(flush_request)) {
        [[maybe_unused]] error_code ec;
        s->wip_timer.cancel(ec);
        wip.reset();
      }
    }

    std::unique_lock lck_rack{s->rack_mtx, std::defer_lock};
    lck_rack.lock();

    // now, to optimize the flush, let's first check if everything racked
    // is within the request.  if so, we can simply clear all racked reels
    auto initial_size = std::ssize(racked);
    if (initial_size > 0) {

      auto first = racked.begin()->second.peek_first();
      auto last = racked.rbegin()->second.peek_last();

      INFO(module_id, fn_id, "rack contains seq_num {:<8}/{:>8} before flush\n", //
           first->seq_num, last->seq_num);

      if (flush_request.matches<frame_t>({first, last})) {
        // yes, the entire rack matches the flush request so we can
        // take a short-cut and clear the entire rack in oneshot
        INFO(module_id, fn_id, "clearing all reels={}\n", initial_size);

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
            INFO(module_id, fn_id, "KEEPING {}\n", reel);
          }
        }
      }

      const auto flushed = initial_size - std::ssize(racked);
      Stats::write(stats::REELS_FLUSHED, int64_t{flushed > 0 ? flushed : initial_size});
    }

    // has everything been flushed?
    if (racked.empty() && !wip) {
      s->first_frame.reset(); // signal we're starting fresh
    }
  });
}

void Racked::init() noexcept {
  if (self.use_count() < 1) {
    self = std::shared_ptr<Racked>(new Racked());

    // initialize supporting objects
    MasterClock::init();
    Anchor::init();
    self->av = Av::create(self->io_ctx);

    static constexpr csv cfg_threads_path{"frame.racked.threads"};
    const int thread_count = config_val<int>(cfg_threads_path, 3);

    auto latch = std::make_shared<std::latch>(thread_count);

    for (auto n = 0; n < thread_count; n++) {
      self->threads.emplace_back([n = n, s = self->ptr(), latch = latch]() mutable {
        name_thread("racked", n);
        latch->arrive_and_wait();
        latch.reset();

        s->io_ctx.run();
      });
    }

    latch->wait(); // caller waits until all threads are started

    INFO(module_id, "init", "sizeof={} frame_sizeof={} lead_time={} fps={}\n", sizeof(Racked),
         sizeof(Frame), pet::humanize(InputInfo::lead_time), InputInfo::fps);
  }
}

void Racked::monitor_wip() noexcept {
  wip_timer.expires_from_now(10s);

  auto s = self->ptr();
  auto &wip_strand = s->wip_strand;

  wip_timer.async_wait( // must run on wip_strand to protect containers
      asio::bind_executor(
          wip_strand, [s = std::move(s), serial_num = wip->serial_num()](const error_code ec) {
            // note: serial_num is captured to ensure the same wip reel is active when
            //       the timer fires

            // save shared_ptr dereferences for multiple use variables
            auto &racked = s->racked;
            auto &wip = s->wip;

            if (!ec && !racked.contains(serial_num)) {
              INFOX(module_id, "MONITOR_WIP", "INCOMPLETE {}\n", wip->inspect());
              Stats::write(stats::RACK_WIP_INCOMPLETE, wip->size());
              // not a timer error and reel is the same
              s->rack_wip();
            } else if (!ec) {
              INFO(module_id, "MONITOR_WIP", "reel serial_num={} already racked\n", serial_num);
            }
          }));
}

frame_future Racked::next_frame() noexcept { // static
  auto prom = frame_promise();
  auto fut = prom.get_future().share();

  // when Racked isn't ready (doesn't exist) return SilentFrame
  if (self.use_count() < 1) {
    prom.set_value(SilentFrame::create());
    return fut;
  }

  auto s = self->ptr();

  // grab a reference to the frame strand since we're moving the shared_ptr
  auto &frame_strand = s->frame_strand;

  asio::post(frame_strand, [s = std::move(s), prom = std::move(prom)]() mutable {
    auto &racked = s->racked; // local reference limit shared_ptr dereferences
    std::unique_lock lck{s->rack_mtx, std::defer_lock}; // don't lock yet

    // do we have any racked reels? if not, return a SilentFrame
    lck.lock(); // guard std::empty(racked) call
    if (std::empty(racked)) {
      prom.set_value(SilentFrame::create());
      return;
    }

    // we have racked reels but need clock_info, unlock while we wait
    lck.unlock();
    auto clock_info = MasterClock::info().get(); // wait for clock info BEFORE locking rack

    if (clock_info.ok() == false) {
      // no clock info, set promise to SilentFrame
      prom.set_value(SilentFrame::create());
    } else [[likely]] {
      // we have ClockInfo and there are racked reels
      // refresh clock info and get anchor info
      clock_info = MasterClock::info().get();
      auto anchor = Anchor::get_data(clock_info);

      // anchor not ready yet or we haven't been instructed to spool
      // so set the promise to a SilentFrame. (i.e. user hit pause, hasn't hit
      // play yet or disconnected)
      //
      // note:  we could have done this check very early and completely avoid
      // the asio::post but didn't so we could prime ClockInfo and Anchor.
      // plus the above code isn't really that expensive every ~23ms.
      if ((anchor.ready() == false) || (s->spool_frames.load() == false)) {
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

        s->log_racked();
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
  static constexpr csv cat{"debug"};

  if (Logger::should_log_info(module_id, cat)) {
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

    if (!msg.empty()) INFO(module_id, cat, "{}\n", msg);
  }
}

void Racked::shutdown() noexcept {

  self->av->shutdown();
  self->av.reset();

  auto s = self->ptr(); // get a fresh shared_ptr to ourselves
  self.reset();         // decrement our local shared_ptr so we're deallocated

  s->guard.reset();

  [[maybe_unused]] error_code ec;
  s->wip_timer.cancel(ec);

  std::for_each(s->threads.begin(), s->threads.end(), [](auto &t) { t.join(); });
  s->threads.clear();

  INFO(module_id, "shutdown", "threads={}\n", std::size(s->threads));

  MasterClock::shutdown();
}

} // namespace pierre