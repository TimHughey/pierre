
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
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/render.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"
#include "config/config.hpp"
#include "frame.hpp"
#include "frame/flush_info.hpp"
#include "master_clock.hpp"
#include "silent_frame.hpp"
#include "stats/stats.hpp"

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

namespace shared {
std::optional<Racked> racked;
}

static std::shared_ptr<frame::Stats> stats;

uint64_t Racked::REEL_SERIAL_NUM{0x1000};

void Racked::accept_frame(frame_t frame) noexcept {
  if (frame->decode()) {
    add_frame(std::move(frame));
  }
}

void Racked::add_frame(frame_t frame) noexcept {
  // notes:
  // 1. frame state guaranteed to be DECODED
  // 2. wip processing is performed a strand to enable queueing of frames
  // 3. queuing can be a problem during a flush
  // 3. so we still use a lock to allow interrupting the queue

  asio::post(wip_strand, [=, this, frame = frame->shared_from_this()]() mutable {
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
}

void Racked::flush(FlushInfo request) { // static
  shared::racked->flush_impl(std::move(request));
}

void Racked::flush_impl(FlushInfo request) {
  // execute this on the wip strand
  asio::post(flush_strand, [=, this]() mutable {
    INFO(module_id, "FLUSH", "{}\n", request.inspect());

    flush_request = request; // record the flush request to check incoming frames

    // flush wip first so we can unlock it quickly
    if (wip.has_value()) {
      // lock both the wip and rack
      std::unique_lock lck_wip(wip_mtx, std::defer_lock);
      lck_wip.lock();

      if (wip->flush(flush_request) && std::ssize(*wip)) {
        wip.reset();
      }
    }

    std::unique_lock lck_rack{rack_mtx, std::defer_lock};
    lck_rack.lock();

    // now, to optimize the flush let's first check if everything racked
    // is within the request.  if so, we can simply clear all racked reels
    auto initial_size = std::ssize(racked);
    if (initial_size > 0) {

      auto first = racked.begin()->second.peek_first();
      auto last = racked.rbegin()->second.peek_last();

      INFO(module_id, "FLUSH", "rack contains seq_num {:<8}/{:>8} before flush\n", //
           first->seq_num, last->seq_num);

      if (flush_request.matches<frame_t>({first, last})) {
        // yes, the entire rack matches the flush request so we can
        // take a short-cut and clear the entire rack in oneshot
        INFO(module_id, "FLUSH", "clearing all reels={}\n", initial_size);
        racked.clear();
      } else {
        // the entire rack did not match the flush request so now
        // we must use maximum effort and look at each reel

        for (auto it = racked.begin(); it != racked.end();) {
          auto &reel = it->second;
          const auto reel_info = reel.inspect();

          if (reel.flush(flush_request)) { // true=reel empty
            it = racked.erase(it);         // returns it to next not erased entry
            stats->write(frame::REELS_RACKED, std::ssize(racked));
          } else {
            INFO(module_id, "FLUSH", "KEEPING {}\n", reel_info);
          }
        }
      }

      if (auto flushed = initial_size - std::ssize(racked); flushed > 0) {
        stats->write(frame::REELS_FLUSHED, flushed);
      }
    }

    // has everything been flushed?
    if (racked.empty() && !wip) {
      first_frame.reset(); // signal we're starting fresh
    }
  });
}

void Racked::handoff(uint8v packet) { // static
  shared::racked->handoff_impl(std::move(packet));
}

void Racked::handoff_impl(uint8v packet) noexcept {
  frame_t frame = Frame::create(packet);

  if (frame->state.header_parsed()) {
    if (flush_request.should_flush(frame)) {
      frame->flushed();
    } else if (frame->decipher(packet)) { // true ensures frame is deciphered
      asio::post(handoff_strand, [=, this]() { accept_frame(frame); });
    }
  }

  if (!frame->deciphered()) {
    INFO(module_id, "HANDOFF", "DISCARDING frame={}\n", frame->inspect());
  }
}

void Racked::init() { // static
  if (shared::racked.has_value() == false) {
    shared::racked.emplace().init_self();
  }
}

void Racked::init_self() {
  const int thread_count = Config().at("frame.racked.threads"sv).value_or(3);

  std::latch latch{thread_count};

  for (auto n = 0; n < thread_count; n++) {
    _threads.emplace_back([=, this, &latch](std::stop_token token) mutable {
      _stop_tokens.add(std::move(token));

      name_thread("Racked", n);
      latch.arrive_and_wait();
      io_ctx.run();
    });
  }

  const auto db_uri = Config().at("stats.db_uri"sv).value_or("unset");
  stats = frame::Stats::init(io_ctx, "RACKED_STATS", db_uri);

  latch.wait(); // caller waits until all threads are started
}

void Racked::monitor_wip() noexcept {
  wip_timer.expires_from_now(10s);

  auto serial_num = wip->serial_num(); // ensure the same wip reel is present when tieer fires

  wip_timer.async_wait( // must run on wip_strand to protect containers
      asio::bind_executor(wip_strand, [=, this](const error_code ec) {
        if (!ec && !racked.contains(serial_num)) {
          INFO(module_id, "MONITOR_WIP", "INCOMPLETE {}\n", wip->inspect());
          // not a timer error and reel is the same
          rack_wip();
        } else if (!ec) {
          INFO(module_id, "MONITOR_WIP", "reel serial_num={} already racked\n", serial_num);
        }
      }));
}

frame_future Racked::next_frame() noexcept { // static
  auto prom = frame_promise();
  auto fut = prom.get_future().share();

  shared::racked->next_frame_impl(std::move(prom));

  return fut;
}

void Racked::next_frame_impl(frame_promise prom) noexcept {

  asio::post(frame_strand, [=, this, prom = std::move(prom)]() mutable {
    std::unique_lock lck{rack_mtx, std::defer_lock}; // don't lock yet

    auto clock_info = MasterClock::info().get(); // wait for clock info BEFORE locking rack
    lck.lock();                                  // need to lock here for std::empty()

    if (!clock_info.ok() || std::empty(racked)) {
      // this is code is here for clarity to avoid losing the plot
      prom.set_value(SilentFrame::create());
    } else [[likely]] {
      auto &reel = racked.begin()->second; // avoid multiple begin()->second and clarity
      auto frame = reel.peek_first();

      // refresh clock info and get anchor info
      clock_info = MasterClock::info().get();
      auto anchor = Anchor::get_data(clock_info);

      // calc the frame state (Frame caches the anchor)
      auto state = frame->state_now(anchor, InputInfo::lead_time);
      prom.set_value(frame);

      if (state.ready() || state.outdated() || state.future()) {
        // consume the ready or outdated frame (rack access is still acquired)
        reel.consume();

        // if the reel is empty, pop it from racked
        if (reel.empty()) {
          racked.erase(racked.begin());
          stats->write(frame::REELS_RACKED, std::ssize(racked));
        }
      }

      log_racked();
    }
  });
}

void Racked::rack_wip() noexcept {
  [[maybe_unused]] error_code ec;
  wip_timer.cancel(ec); // wip reel is full, cancel the incomplete spool timer

  log_racked_rc rc{NONE};

  const auto wip_info = wip->inspect();
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

    if (rc == COLLISION) stats->write(frame::RACK_COLLISION, true);
    if (rc == RACKED) stats->write(frame::REELS_RACKED, std::ssize(racked));
    if (rc == TIMEOUT) stats->write(frame::RACK_WIP_TIMEOUT, true);

    log_racked(wip_info, rc);

    // regardless of the outcome above, reset wip
    wip.reset();
  }
}

void Racked::log_racked(const string &wip_info, log_racked_rc rc) const noexcept {
  string msg;
  auto w = std::back_inserter(msg);

  auto reel_count = std::ssize(racked);

  switch (rc) {
  case log_racked_rc::NONE:
    if (reel_count == 0) {
      auto frame_count = racked.cbegin()->second.size();
      if (frame_count == 1) fmt::format_to(w, "LAST FRAME\n");
    } else if ((reel_count >= 400) && ((reel_count % 10) == 0)) {
      fmt::format_to(w, "HIGH REELS reels={:<3}", reel_count);
    }

    break;

  case log_racked_rc::RACKED:
    if ((reel_count == 1) && !wip_info.empty()) {
      fmt::format_to(w, "FIRST REEL reels={:<3} wip_reel={}", 1, wip_info);
    } else if ((reel_count == 2) && !wip_info.empty()) {
      fmt::format_to(w, "RACKED     reels={:<3} wip_reel={}", reel_count, wip_info);
    }
    break;

  default:
    break;
  }

  if (!msg.empty()) INFO(module_id, "LOG_RACKED", "{}\n", msg);
}

} // namespace pierre