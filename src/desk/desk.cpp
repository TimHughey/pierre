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

#include "desk/desk.hpp"
#include "base/conf/token.hpp"
#include "base/conf/toml.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/stats.hpp"
#include "desk/async/write.hpp"
#include "desk/msg/data.hpp"
#include "dmx_ctrl.hpp"
#include "frame/anchor.hpp"
#include "frame/anchor_last.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"
#include "frame/master_clock.hpp"
#include "frame/racked.hpp"
#include "frame/state.hpp"
#include "fx/all.hpp"
#include "mdns/mdns.hpp"

#include <boost/asio/append.hpp>
#include <boost/asio/post.hpp>
#include <fmt/chrono.h>
#include <fmt/std.h>
#include <functional>
#include <ranges>
#include <thread>

namespace pierre {

// notes:
//  -must be defined in .cpp (incomplete types in .hpp)
//  -we use a separate io_ctx for desk and it's workers
Desk::Desk(MasterClock *master_clock) noexcept
    : tokc(module_id),
      // create and start racked early (frame rendering needs it)
      racked{std::make_unique<Racked>()},
      // create DmxCtrl early (frame renderings needs it)
      dmx_ctrl(std::make_unique<desk::DmxCtrl>(io_ctx)),
      // default to not rendering (frame rendering needs it)
      render_active{false},
      // cache the MasterClock pointer early (frame rendering needs it)
      master_clock(master_clock),
      // create the anchor early (frame rendering needs it)
      anchor(std::make_unique<Anchor>()),
      // create a strand to serialize render activities
      render_strand(asio::make_strand(io_ctx)),
      // set the frame_timer start time to now, we'll adjust it later
      frame_timer(render_strand, steady_clock::now()) //
{
  io_ctx.stop(); // needed by resume
  resume();
}

Desk::~Desk() noexcept {
  INFO_AUTO_CAT("destruct");

  io_ctx.stop();

  INFO_AUTO("io_ctx.stopped()={}", io_ctx.stopped());
}

////
//// API
///

void Desk::anchor_reset() noexcept { anchor->reset(); }
void Desk::anchor_save(AnchorData &&ad) noexcept { anchor->save(std::forward<AnchorData>(ad)); }

void Desk::flush(FlushInfo &&request) noexcept {

  asio::post(render_strand, [this, fr = request]() mutable { active_reel.flush(fr); });

  if (racked) racked->flush(std::forward<FlushInfo>(request));
}

void Desk::flush_all() noexcept {
  if (racked) racked->flush_all();
}

bool Desk::fx_finished() const noexcept { return active_fx ? active_fx->completed() : true; }

void Desk::fx_select(Frame &frame) noexcept {
  static constexpr csv fn_id{"fx_select"};

  // cache multiple use frame info
  const auto silent = frame.silent();

  const auto fx_now = active_fx ? active_fx->name() : desk::fx::NONE;
  const auto fx_suggested_next = active_fx ? active_fx->suggested_fx_next() : desk::fx::STANDBY;

  // when fx::Standby is finished initiate standby()
  if (fx_now == desk::fx::NONE) { // default to Standby
    active_fx = std::make_unique<desk::Standby>(render_strand);
  } else if (silent && (fx_suggested_next == desk::fx::STANDBY)) {
    active_fx = std::make_unique<desk::Standby>(render_strand);

  } else if (!silent && (frame.state.ready_or_future())) {
    active_fx = std::make_unique<desk::MajorPeak>(render_strand);

  } else if (silent && (fx_suggested_next == desk::fx::ALL_STOP)) {
    active_fx = std::make_unique<desk::AllStop>(render_strand);
  }

  // note in log selected FX, if needed
  if (!active_fx->match_name(fx_now)) INFO_AUTO("FX {} -> {}\n", fx_now, active_fx->name());
}

void Desk::handoff(uint8v &&packet, const uint8v &key) noexcept {
  if (racked) racked->handoff(std::forward<uint8v>(packet), key);
}

void Desk::next_reel(Nanos wait_ns) noexcept {
  INFO_AUTO_CAT("next_real");

  // when processing a synthetic reel of frames and render is active
  // we always attempt to load the next reel
  if (active_reel.empty() || (active_reel.synthetic() && render_active.load(mem_order::relaxed))) {

    // we need a new reel now
    if (load_reel(wait_ns) == false) {
      INFO_AUTO("failed, falling back to synthetic");
      // hmmm, we couldn't load a reel, fallback to synthetic
      active_reel = Reel(Reel::Synthetic);
    }
  }
}

bool Desk::load_reel(const Nanos wait_ns) noexcept {
  INFO_AUTO_CAT("load_reel");

  auto rc{false};

  // do we have a pending future reel?
  if (future_reel.valid()) {
    using fstatus = std::future_status;

    if (auto status = future_reel.wait_for(wait_ns); status == fstatus::ready) {
      rc = true;
      std::exchange(active_reel, racked->next_reel().get());
    } else
      INFO_AUTO("timeout");
  } else {
    future_reel = racked->next_reel();
  }

  return rc;
}

void Desk::render_in_strand() noexcept {

  // get a frame from the reel
  auto frame = active_reel.peek_or_pop(*master_clock, anchor.get());

  auto sync_wait = frame.sync_wait();

  frame_timer.expires_after(sync_wait);
  frame_timer.async_wait(asio::append(
      [this](const error_code &ec, Frame frame) {
        if (ec) return;

        Elapsed e;
        frame.record_state();

        if (fx_finished()) fx_select(frame);

        if (shutdown_if_all_stop()) return;

        // render frame and send to DMX controller
        if (frame.state.ready()) {
          desk::DataMsg msg(frame.seq_num, frame.silent());

          // send the data message if rendered
          if (active_fx->render(frame.get_peaks(), msg)) {
            dmx_ctrl->send_data_msg(std::move(msg));
          }

        } // end ready frame handling

        // notate the frame is rendered (or silent) in timeseries db
        frame.mark_rendered();

        next_reel(2ms);

        render_in_strand();
        Stats::write(stats::RENDER_ELAPSED, e.as<Nanos>());
      },
      std::move(frame)));
}

void Desk::resume() noexcept {
  INFO_AUTO_CAT("resume");

  if (io_ctx.stopped()) {
    INFO_AUTO("invoked, io_ctx.stopped={}", io_ctx.stopped());

    if (threads.size()) {
      // ensure any previous threads are released
      std::for_each(threads.begin(), threads.end(), [this](std::jthread &t) {
        INFO("threads", "detach id={}", t.get_id());

        t.detach();
      });

      threads.clear();
      INFO_AUTO("cleared {} threads", threads.size());
    }

    INFO_AUTO("io_ctx stopped, resetting...");
    io_ctx.reset();

    // add work to io_ctx before starting threads
    frame_timer.expires_at(steady_clock::now());
    frame_timer.async_wait([this](const error_code &ec) {
      if (ec) return;

      // start with a 500ms reel of synthetic frames
      auto prom = std::promise<Reel>();
      prom.set_value(Reel(Reel::Synthetic));

      // ensure we have a clean Anchor by resetting it (not freeing the unique_ptr)
      anchor->reset();

      render_in_strand();
    });

    const auto n_thr = tokc.val<int>("threads"_tpath, 1);
    INFO_AUTO("starting {} threads...", n_thr);

    // add threads to the io_ctx and start
    for (auto n = 0; n < n_thr; ++n) {
      threads.emplace_back(
          // add threads for handling frame rendering
          std::jthread([this, n = n]() {
            {
              const string tname{fmt::format("pierre_desk{}", n)};
              pthread_setname_np(pthread_self(), tname.c_str());
              INFO("threads", "started {}", tname);
            }

            io_ctx.run();
          }));
    }

    using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;

    auto lt_min = std::chrono::duration_cast<millis_fp>(InputInfo::lead_time_min);
    INFO_INIT("sizeof={:>5} lead_time_min={:0.4}", sizeof(Desk), lt_min);
  }
}

void Desk::set_render(bool enable) noexcept {
  INFO_AUTO_CAT("set_render");

  auto was_active = std::atomic_exchange_explicit(&render_active, enable, mem_order::relaxed);

  INFO_AUTO("enable={} was_active={}", enable, was_active);

  if (was_active == false) {

    const auto now = steady_clock::now();

    frame_timer.expires_at(now);
    frame_timer.async_wait([this](const error_code &ec) {
      if (!ec) asio::post(io_ctx, [this]() { render(); });
    });
  }
}

bool Desk::shutdown_if_all_stop() noexcept {

  auto rc = active_fx->match_name(desk::fx::ALL_STOP);

  if (rc) {

    asio::dispatch(render_strand, [this]() {
      INFO_AUTO_CAT("shutdown_all");

      io_ctx.stop();
      INFO_AUTO("active_fx={} io_ctx.stopped={}", active_fx->name(), io_ctx.stopped());

      // halt the frame timer
      [[maybe_unused]] error_code ec;
      frame_timer.cancel(ec);

      // ensure we are not attempting to render actual audio data
      set_render(false);

      // disconnect from ruth
      dmx_ctrl.reset();

      // leave racked running and flush pending reels
      racked->flush_all();

      // reset the allocated Anchor, not the unique_ptr
      anchor->reset();
    });
  }

  return rc;
}

} // namespace pierre
