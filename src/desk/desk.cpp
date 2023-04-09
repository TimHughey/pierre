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
#include "frame/racked.hpp"
#include "frame/silent_frame.hpp"
#include "frame/state.hpp"
#include "fx/all.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"
#include <frame/master_clock.hpp>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <functional>
#include <ranges>

namespace pierre {

// must be defined in .cpp to hide mdns
Desk::Desk(MasterClock *master_clock) noexcept
    : // setup the thread pool
      thread_count(config_threads<Desk>(1)), thread_pool(thread_count),
      // TODO:  do we need a work guard?
      guard(asio::make_work_guard(thread_pool)),
      // set the frame_timer start time to now, we'll adjust it later
      frame_timer(thread_pool, system_clock::now()),
      // ensure Racked is not allocated
      racked{nullptr},
      // cache the MasterClock pointer
      master_clock(master_clock) //
{
  INFO_INIT("sizeof={:>5} lead_time_min={} render_active.is_lock_free={}\n", sizeof(Desk),
            pet::humanize(InputInfo::lead_time_min), render_active.is_lock_free());

  resume();
}

Desk::~Desk() noexcept {
  thread_pool.stop();
  thread_pool.join();
}

// general API

void Desk::anchor_reset() noexcept { anchor->reset(); }
void Desk::anchor_save(AnchorData &&ad) noexcept { anchor->save(std::forward<AnchorData>(ad)); }

void Desk::flush(FlushInfo &&request) noexcept {

  asio::post(thread_pool, [this, fr = request]() mutable { active_reel->flush(fr); });

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
    active_fx = std::make_unique<desk::Standby>(thread_pool);
  } else if (silent && (fx_suggested_next == desk::fx::STANDBY)) {
    active_fx = std::make_unique<desk::Standby>(thread_pool);

  } else if (!silent && (frame->state.ready_or_future())) {
    active_fx = std::make_unique<desk::MajorPeak>(thread_pool);

  } else if (silent && (fx_suggested_next == desk::fx::ALL_STOP)) {
    active_fx = std::make_unique<desk::AllStop>(thread_pool);
  }

  // note in log selected FX, if needed
  if (!active_fx->match_name(fx_now)) INFO_AUTO("FX {} -> {}\n", fx_now, active_fx->name());
}

void Desk::handoff(uint8v &&packet, const uint8v &key) noexcept {
  if (racked) racked->handoff(std::forward<uint8v>(packet), key);
}

void Desk::next_reel() noexcept {
  if (render_active.load(std::memory_order_relaxed) && active_reel->empty()) {
    set_active_reel(std::move(racked->next_reel().get()));
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

      if (active_fx->render(frame->get_peaks(), msg)) {
        if (!dmx_ctrl) dmx_ctrl = std::make_unique<desk::DmxCtrl>();

        dmx_ctrl->send_data_msg(std::move(msg));
      }

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
        INFO_AUTO("adjusting frame sync seq_num={} {}\n", //
                  frame->seq_num, pet::as<Micros>(sync_wait - InputInfo::lead_time));
        next_at = last_at + sync_wait;
      }
    }

    // first, set the frame_timer to fire for the next frame
    frame_timer.expires_at(next_at);
    frame_timer.async_wait([this](const error_code &ec) {
      if (!ec) render();
    });

    Stats::write(stats::RENDER_ELAPSED, (Nanos)render_elapsed);
  } // end frame processing when not shutdown
}

void Desk::render_start() noexcept {

  // at start-up ensure the frame timer fires immediately
  const auto now = system_clock::now();
  if (frame_timer.expiry() > now) frame_timer.expires_at(now);

  frame_timer.async_wait([this](const error_code &ec) {
    if (!ec) {
      asio::post(thread_pool, [this]() {
        // we use asio::post here to ensure the frame_timer next expiry is set
        // outside of this completion handler
        render();
      });
    }
  });
}

void Desk::resume() noexcept {
  static constexpr csv fn_id{"resume"};

  INFO_AUTO("requested, thread_count={}\n", thread_count);

  asio::dispatch(thread_pool, [this]() {
    std::exchange(anchor, std::make_unique<Anchor>());
    std::exchange(racked, std::make_unique<Racked>()); // ensure Racked is running
    std::exchange(active_reel, std::make_unique<Reel>());

    render();
  });

  INFO_AUTO("completed, thread_count={}\n", thread_count);
}

bool Desk::shutdown_if_all_stop() noexcept {

  auto rc = active_fx->match_name(desk::fx::ALL_STOP);

  if (rc) {

    // halt the frame timer
    [[maybe_unused]] error_code ec;
    frame_timer.cancel(ec);

    // shutdown supporting subsystems
    active_fx.reset();
    dmx_ctrl.reset();
    active_reel.reset();
    racked.reset();
    anchor.reset();
  }

  return rc;
}

} // namespace pierre
