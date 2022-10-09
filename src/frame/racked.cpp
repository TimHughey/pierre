
/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "racked.hpp"
#include "anchor.hpp"
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "frame.hpp"
#include "frame/flush_info.hpp"
#include "master_clock.hpp"

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

uint64_t Racked::REEL_SERIAL_NUM{0x1000};

void Racked::impl_anchor_save(bool render, AnchorData &&ad) {
  impl_adjust_render_mode(render);

  Anchor::save(std::forward<AnchorData>(ad));
}

void Racked::impl_flush(FlushInfo request) {

  // execute this on the wip strand
  asio::post(wip_strand, [=, this]() mutable {
    flush_request = request; // record the flush request
    __LOG0(LCOL01 " REQUEST {}\n", module_id, "FLUSH", request.inspect());

    // now, to optimize the flush let's first check if everything racked
    // is within the request.  if so, we can simply clear all racked reels
    if (size() > 0) {
      racked_acquire();

      auto first = racked.front().peek_first();
      auto last = racked.back().peek_last();

      __LOG0(LCOL01 " rack contains seq_num {:<8}/{:>8}\n", module_id, "FLUSH", first->seq_num,
             last->seq_num);

      if (flush_request.matches<frame_t>({first, last})) {
        // yes, all racked reels are included in the flush request
        __LOG0(LCOL01 " clearing all\n", module_id, "FLUSH");
        racked.clear();
      } else {
        // ok, at least some reels contain frames to flush.
        // let's do a reel by reel flush

        for (auto it = racked.begin(); it != racked.end();) {
          const auto reel_info = it->inspect();

          if (it->flush(flush_request).empty()) {
            it++;
            racked.pop_front();
          } else {
            __LOG0(LCOL01 " keeping {}\n", module_id, "FLUSH", reel_info);
          }
        }
      }

      update_racked_size();
      update_reel_ready();
      racked_release();

      // finally, flush the wip reel
      if (reel_wip.has_value()) {
        if (auto keep = reel_wip->flush(flush_request); keep == false) {
          reel_wip.reset();
        }
      }
    }
  });
}

void Racked::impl_handoff(uint8v &packet) {
  frame_t frame = Frame::create(packet);

  // allow the calling thread to perform the av parsing
  frame->parse();

  if (frame->state.decoded()) {
    asio::post(wip_strand, [=, this]() {
      // NOTE: FlushInfo::should_keep() will complete the flush request
      //       when the seq_num is outside of the request
      if (flush_request.should_keep(frame)) {

        // create the wip reel, if needed
        if (reel_wip.has_value() == false) reel_wip.emplace(++REEL_SERIAL_NUM);

        frame->process();
        reel_wip->add(frame);

        if (reel_wip->full()) {
          rack_wip();
        } else if (reel_wip->size() == 1) {
          monitor_wip();
        }
      }
    });
  }
}

void Racked::impl_next_frame(const Nanos lead_time, frame_promise prom) noexcept {
  asio::post(io_ctx, [=, this, prom = std::move(prom)]() mutable {
    // wait for clock to become available.
    // only a delay when:
    //  1.  at startup before first set anchor
    //  2.  master clock has changed and isn't stable
    auto clock_info = MasterClock::info().get();

    wait_for_reel();

    if (racked_acquire()) {
      if (!empty()) { // a flush request may have cleared all reels
        auto frame = racked.front().peek_first();

        // get anchor data, hopefully clock is ready by this point
        auto anchor = shared::anchor->get_data(clock_info);

        // calc the frame state (Frame caches the anchor)
        auto state = frame->state_now(anchor, lead_time);
        prom.set_value(frame);
        frame.reset(); // done with this frame

        if (state.ready() || state.outdated()) {
          // consume the ready or outdated frame (rack access is still acquired)
          racked.front().consume();
          pop_front_reel_if_empty();
        }
      } else {
        prom.set_value(frame_t());
      }

      racked_release(); // we're done with the rack
    } else {
      __LOG0(LCOL01 " waiting for racked reels\n", module_id, "TIMEOUT");
      prom.set_value(frame_t());
    }
  });
}

void Racked::init() { // static
  if (shared::racked.has_value() == false) {
    shared::racked.emplace().init_self();
  }
}

void Racked::init_self() {
  std::latch latch{THREAD_COUNT};

  for (auto n = 0; n < THREAD_COUNT; n++) {
    // notes:
    //  1. start DSP processing threads
    _threads.emplace_back([=, this, &latch](std::stop_token token) {
      _stop_tokens.add(std::move(token));

      name_thread(module_id, n);
      latch.arrive_and_wait();
      io_ctx.run();
    });
  }

  latch.wait(); // caller waits until all threads are started
}

void Racked::monitor_wip() noexcept {
  wip_timer.expires_from_now(10s);

  auto serial_num = reel_wip->_serial; // ensure the same wip reel is present when tieer fires

  wip_timer.async_wait( // must run on wip_strand to protect containers
      asio::bind_executor(wip_strand, [=, this](const error_code ec) {
        if (!ec && (reel_wip == serial_num)) {
          __LOG0(LCOL01 " INCOMPLETE {}\n", module_id, "MONITOR_WIP", reel_wip->inspect());
          // not a timer error and reel is the same
          rack_wip();
        } else if (!ec) {
          __LOG0(LCOL01 " expected reel={:#4x}, have reel={:#4x}\n", module_id, "MONITOR_WIP",
                 reel_wip->_serial, serial_num);
        }
      }));
}

// NOTE: this function assumes are locked
void Racked::pop_front_reel_if_empty() noexcept {
  // if the reel is empty, pop it from racked
  if (size() > 0) {

    if (racked.front().empty()) {
      const auto reel_num = racked.front().serial_num();
      racked.pop_front();
      update_racked_size();

      log_racked();
    }
  }
}

void Racked::rack_wip() noexcept {
  [[maybe_unused]] error_code ec;
  wip_timer.cancel(ec); // wip reel is full, cancel the incomplete spool timer

  // when this is the first reel we do not racked_require() because it
  // is initialized to held
  if (empty() || racked_acquire(InputInfo::lead_time())) {
    const auto wip_info = reel_wip->inspect();

    if (!reel_wip->empty()) {

      racked.emplace_back(std::move(reel_wip.value()));
      update_racked_size();
      racked_release();

      log_racked(wip_info);
      update_reel_ready();

      reel_wip.reset();
    }

  } else {
    __LOG0(LCOL01 " TIMEOUT while racking {}\n", module_id, "RACK_WIP", reel_wip->inspect());
  }
}

void Racked::update_reel_ready() noexcept {
  if (size() == 0) {
    if (reel_ready.try_acquire_for(1ms) == false) {
      __LOG0(LCOL01 " forcing reel ready to acquired\n", module_id, "RACKED");

      reel_ready.release();
    }

    reel_ready.acquire();

  } else {
    reel_ready.release();
  }
}

//
// misc logging and debug
//

void Racked::log_racked(const string &wip_info) const noexcept {
  string msg;
  auto w = std::back_inserter(msg);

  auto reel_count = size();

  if ((reel_count < 3) && wip_info.empty()) {
    fmt::format_to(w, "LOW REELS   reels={:<3} wip_reel=<empty>", reel_count);
  } else if ((reel_count >= 130) && ((reel_count % 10) == 0) && !wip_info.empty()) {
    fmt::format_to(w, "HIGH REELS  reels={:<3} wip_reel={}", reel_count, wip_info);
  } else if ((reel_count == 1) && !wip_info.empty()) {
    fmt::format_to(w, "FIRST REEL  reels={:<3} wip_reel={}", 1, wip_info);
  } else if ((reel_count == 2) && !wip_info.empty()) {
    fmt::format_to(w, "RACKED      reels={:<3} wip_reel={}", reel_count, wip_info);
  }

  if (!msg.empty()) __LOG0(LCOL01 " {}\n", module_id, "LOG_RACKED", msg);
}

} // namespace pierre