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

#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <functional>
#include <ranges>

namespace pierre {

// must be defined in .cpp to hide mdns
Desk::Desk(MasterClock *master_clock) noexcept
    : thread_count(config_threads<Desk>(1)), thread_pool(thread_count),
      guard(asio::make_work_guard(thread_pool)), frame_strand(asio::make_strand(thread_pool)),
      sync_strand(asio::make_strand(thread_pool)), frame_timer(sync_strand), racked{nullptr},
      master_clock(master_clock), anchor(std::make_unique<Anchor>()) //
{
  INFO_INIT("sizeof={:>5} lead_time_min={}\n", sizeof(Desk),
            pet::humanize(InputInfo::lead_time_min));

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
  if (racked) racked->flush(std::forward<FlushInfo>(request));
}

void Desk::flush_all() noexcept {
  if (racked) racked->flush_all();
}

void Desk::frame_loop() noexcept {
  static constexpr csv fn_id{"frame_loop"};

  racked->next_frame(frame_strand, [this, next_wait = Elapsed()](frame_t frame) mutable {
    if (next_wait.freeze() > 100us) {
      Stats::write(stats::NEXT_FRAME_WAIT, next_wait.freeze());
    }

    // we are assured the frame can be rendered (slient or peaks)
    handle_frame(frame);
  });
}

bool Desk::fx_finished() const noexcept { return active_fx ? active_fx->completed() : true; }

void Desk::fx_select(const frame::state &frame_state, bool silent) noexcept {
  static constexpr csv fn_id{"fx_select"};

  const auto fx_now = active_fx ? active_fx->name() : desk::fx::NONE;
  const auto fx_suggested_next = active_fx ? active_fx->suggested_fx_next() : desk::fx::STANDBY;

  // when fx::Standby is finished initiate standby()
  if (fx_now == desk::fx::NONE) { // default to Standby
    active_fx = std::make_unique<desk::Standby>(frame_strand);
  } else if (silent && (fx_suggested_next == desk::fx::STANDBY)) {
    active_fx = std::make_unique<desk::Standby>(frame_strand);

  } else if (!silent && (frame_state.ready() || frame_state.future())) {
    active_fx = std::make_unique<desk::MajorPeak>(frame_strand);

  } else if (silent && (fx_suggested_next == desk::fx::ALL_STOP)) {
    active_fx = std::make_unique<desk::AllStop>(frame_strand);
  }

  // note in log selected FX, if needed
  if (!active_fx->match_name(fx_now)) {
    INFO_AUTO("FX {} -> {}\n", fx_now, active_fx->name());
  }
}

void Desk::handle_frame(frame_t frame) noexcept {
  frame->state.record_state();

  if (fx_finished()) fx_select(frame->state, frame->silent());

  if (active_fx->match_name(desk::fx::ALL_STOP)) {
    // shutdown supporting subsystems
    active_fx.reset();
    dmx_ctrl.reset();
    racked.reset();
    return;
  }

  // render this frame and send to DMX controller
  if (frame->state.ready()) {
    desk::DataMsg msg(frame->seq_num, frame->silent());

    if (active_fx->render(frame, msg)) {
      if (!dmx_ctrl) dmx_ctrl = std::make_unique<desk::DmxCtrl>();

      dmx_ctrl->send_data_msg(std::move(msg));
    }
  }

  // notates rendered or silence in timeseries db
  frame->mark_rendered();

  // now we need to wait for the correct time to render the next frame
  if (auto sync_wait = frame->sync_wait_recalc(); sync_wait > Nanos::zero()) {
    static constexpr csv fn_id{"desk.frame_loop"};

    frame_timer.expires_after(sync_wait);
    frame_timer.async_wait([this](const error_code &ec) {
      if (!ec) asio::dispatch(frame_strand, [this]() { frame_loop(); });
    });
  } else {
    // INFO_AUTO("negative sync wait {}\n", frame->sync_wait());
    asio::post(frame_strand, [this]() { frame_loop(); });
  }
}

void Desk::handoff(uint8v &&packet, const uint8v &key) noexcept {
  if (racked) racked->handoff(std::forward<uint8v>(packet), key);
}

void Desk::resume() noexcept {
  static constexpr csv fn_id{"resume"};

  INFO_AUTO("requested, thread_count={}\n", thread_count);

  asio::dispatch(frame_strand, [this]() {
    // ensure Racked is running
    std::exchange(racked, std::make_unique<Racked>(master_clock, anchor.get()));

    // start frame_loop()
    frame_loop();
  });

  INFO_AUTO("completed, thread_count={}\n", thread_count);
}

void Desk::spool(bool enable) noexcept {
  //// TODO: check this!
  if (racked) racked->spool(enable);
}

} // namespace pierre
