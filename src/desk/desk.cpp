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
#include "base/input_info.hpp"
#include "base/pet.hpp"
#include "desk/async/write.hpp"
#include "desk/msg/data.hpp"
#include "dmx_ctrl.hpp"
#include "frame/anchor.hpp"
#include "frame/anchor_last.hpp"
#include "frame/flush_info.hpp"
#include "frame/frame.hpp"
#include "frame/master_clock.hpp"
#include "frame/racked.hpp"
#include "frame/silent_frame.hpp"
#include "frame/state.hpp"
#include "fx/all.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"

#include <boost/asio/post.hpp>
#include <fmt/chrono.h>
#include <functional>
#include <ranges>
#include <thread>

namespace pierre {

// notes:
//  -must be defined in .cpp (incomplete types in .hpp)
//  -we use a separate io_ctx for desk and it's workers
Desk::Desk(MasterClock *master_clock) noexcept
    : // create and start racked early (frame rendering needs it)
      racked{std::make_unique<Racked>()},
      // create DmxCtrl early (frame renderings needs it)
      dmx_ctrl(std::make_unique<desk::DmxCtrl>(io_ctx)),
      // default to not rendering (frame rendering needs it)
      render_active{false},
      // cache the MasterClock pointer early (frame rendering needs it)
      master_clock(master_clock),
      // create the anchor early (frame rendering needs it)
      anchor(std::make_unique<Anchor>()),
      // start with a silent Reel
      active_reel(std::make_unique<Reel>()),
      // create a strand to serialize render activities
      render_strand(asio::make_strand(io_ctx)),
      // set the frame_timer start time to now, we'll adjust it later
      frame_timer(render_strand, steady_clock::now()) //
{

  // include the threads DmxCtrl requires
  const auto threads = config_threads<Desk>(1) + dmx_ctrl->required_threads();

  INFO_INIT("sizeof={:>5} lead_time_min={} threads={}\n", sizeof(Desk),
            pet::humanize(InputInfo::lead_time_min), threads);

  resume(); // add work to io_ctx

  // add threads to the io_ctx and start
  for (auto n = 0; n < threads; n++) {
    // add threads for handling frame rendering
    std::jthread([io_ctx = std::ref(io_ctx)]() { io_ctx.get().run(); }).detach();
  }
}

Desk::~Desk() noexcept { io_ctx.stop(); }

////
//// API
///

void Desk::anchor_reset() noexcept { anchor->reset(); }
void Desk::anchor_save(AnchorData &&ad) noexcept { anchor->save(std::forward<AnchorData>(ad)); }

void Desk::flush(FlushInfo &&request) noexcept {

  asio::post(render_strand, [this, fr = request]() mutable { active_reel->flush(fr); });

  if (racked) racked->flush(std::forward<FlushInfo>(request));
}

void Desk::flush_all() noexcept {
  if (racked) racked->flush_all();
}

bool Desk::fx_finished() const noexcept { return active_fx ? active_fx->completed() : true; }

void Desk::fx_select(Frame *frame) noexcept {
  static constexpr csv fn_id{"fx_select"};

  // cache multiple use frame info
  const auto silent = frame->silent();

  const auto fx_now = active_fx ? active_fx->name() : desk::fx::NONE;
  const auto fx_suggested_next = active_fx ? active_fx->suggested_fx_next() : desk::fx::STANDBY;

  // when fx::Standby is finished initiate standby()
  if (fx_now == desk::fx::NONE) { // default to Standby
    active_fx = std::make_unique<desk::Standby>(render_strand);
  } else if (silent && (fx_suggested_next == desk::fx::STANDBY)) {
    active_fx = std::make_unique<desk::Standby>(render_strand);

  } else if (!silent && (frame->state.ready_or_future())) {
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

void Desk::next_reel() noexcept {
  if (render_active.load(std::memory_order_relaxed) && active_reel->empty()) {
    auto reel = racked->next_reel().get();

    std::exchange(active_reel, std::move(reel));
  }
}

void Desk::render() noexcept {
  static constexpr csv fn_id{"render"};

  Elapsed render_elapsed;

  // get the next reel, if needed
  next_reel();

  // now handle the frame for this render
  const auto clock_info = master_clock->info_no_wait();

  auto reel_result = active_reel->peek_or_pop(clock_info, anchor.get());
  auto frame = reel_result.frame_ptr();

  frame->state.record_state();

  if (fx_finished()) fx_select(frame);

  if (shutdown_if_all_stop() == false) {

    // render frame and send to DMX controller
    if (frame->state.ready()) {
      desk::DataMsg msg(frame->seq_num, frame->silent());

      // send the data message if rendered
      if (active_fx->render(frame->get_peaks(), msg)) dmx_ctrl->send_data_msg(std::move(msg));

      // we do not want to include the time to write stats or
      // request the next reel in render elapsed
      render_elapsed.freeze();

      // get the next reel if we emptied this reel
      next_reel();

      // notate the frame is rendered (or silent) in timeseries db
      frame->mark_rendered();
    } // end ready frame handling

    // begin calculation of when next frame timer should fire
    const auto last_at = frame_timer.expiry();
    auto next_at = last_at + InputInfo::lead_time;

    // when the frame is in the future override next_at avoid sending silence frames
    // and to maximize sync
    if (frame->state.future()) {
      if (const auto sync_wait = frame->sync_wait_recalc(); sync_wait > InputInfo::lead_time) {
        const auto adjust = sync_wait - InputInfo::lead_time;
        Stats::write(stats::FRAME_TIMER_ADJUST, adjust);

        next_at = last_at + sync_wait;
      }
    }

    // set the frame_timer to fire for the next frame
    frame_timer.expires_at(next_at);
    frame_timer.async_wait([this](const error_code &ec) {
      if (!ec) render();
    });

    Stats::write(stats::RENDER_ELAPSED, (Nanos)render_elapsed);
  } // end frame processing when not shutdown
}

void Desk::resume() noexcept {
  static constexpr csv fn_id{"resume"};

  if (io_ctx.stopped()) {
    INFO_AUTO("io_ctx stopped, resetting\n");
    io_ctx.reset();
  }

  frame_timer.expires_at(steady_clock::now());
  frame_timer.async_wait([this](const error_code &ec) {
    if (ec) return;

    // ensure we have a clean Anchor by resetting it (not freeing the unique_ptr)
    anchor->reset();

    render();
  });
}

bool Desk::shutdown_if_all_stop() noexcept {
  auto rc = active_fx->match_name(desk::fx::ALL_STOP);

  if (rc) {

    // halt the frame timer
    [[maybe_unused]] error_code ec;
    frame_timer.cancel(ec);

    // ensure we are not attempting to render actual audio data
    set_render(false);

    // disconnect from ruth
    dmx_ctrl.reset();

    // free the active reel
    active_reel.reset();

    // leave racked running and flush pending reels
    racked->flush_all();

    // reset the allocated Anchor, not the unique_ptr
    anchor->reset();
  }

  return rc;
}

} // namespace pierre
