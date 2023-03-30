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
#include "base/thread_util.hpp"
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
#include "io/post.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"

#include <exception>
#include <functional>
#include <future>
#include <latch>
#include <math.h>
#include <ranges>
#include <tuple>

namespace pierre {

// must be defined in .cpp to hide mdns
Desk::Desk(MasterClock *master_clock) noexcept
    : frame_timer(io_ctx),                  //
      racked{nullptr},                      //
      master_clock(master_clock),           //
      anchor(std::make_unique<Anchor>()),   //
      state{Stopped},                       //
      thread_count(config_threads<Desk>(2)) //
{
  INFO_INIT("sizeof={:>5} lead_time_min={}\n", sizeof(Desk),
            pet::humanize(InputInfo::lead_time_min));

  resume();
}

Desk::~Desk() noexcept {
  INFO_SHUTDOWN_REQUESTED();

  standby();

  if (shutdown_latch) shutdown_latch->wait();

  INFO_SHUTDOWN_COMPLETE();
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

void Desk::frame_loop(bool fx_finished) noexcept {
  static constexpr csv fn_id{"frame_loop"};

  if (state != Running) {
    standby();
    return;
  }

  racked->next_frame(io_ctx, [this, fx_finished, next_wait = Elapsed()](frame_t frame) mutable {
    // we are assured the frame can be rendered (slient or peaks)

    frame->state.record_state();

    if (fx_finished) fx_select(frame->state, frame->silent());

    // render this frame and send to DMX controller
    if (frame->state.ready()) {
      desk::DataMsg msg(frame->seq_num, frame->silent());

      if (active_fx) {
        fx_finished = active_fx->render(frame, msg);

        if (!fx_finished) {
          if (!dmx_ctrl) dmx_ctrl = std::make_unique<desk::DmxCtrl>();

          dmx_ctrl->send_data_msg(std::move(msg));
        }
      }
    }

    // notates rendered or silence in timeseries db
    frame->mark_rendered();

    // now we need to wait for the correct time to render the next frame
    if (auto sync_wait = frame->sync_wait_recalc(); sync_wait > Nanos::zero()) {
      static constexpr csv fn_id{"desk.frame_loop"};

      frame_timer.expires_after(sync_wait);
      frame_timer.async_wait([this, fx_finished](const error_code &ec) {
        if (!ec) frame_loop(fx_finished);
      });
    } else {
      // INFO_AUTO("negative sync wait {}\n", frame->sync_wait());
      asio::post(io_ctx, [this, fx_finished]() { frame_loop(fx_finished); });
    }

    if (state == Stopping) standby();

    if (next_wait.freeze() > 100us) {
      Stats::write(stats::NEXT_FRAME_WAIT, next_wait.freeze());
    }
  });
}

void Desk::fx_select(const frame::state &frame_state, bool silent) noexcept {
  static constexpr csv fn_id{"fx_select"};

  const auto fx_now = active_fx ? active_fx->name() : csv{"none"};
  const auto fx_suggested_next = active_fx ? active_fx->suggested_fx_next() : desk::fx::STANDBY;

  if (active_fx) active_fx->cancel(); // stop any pending io_ctx work

  // when fx::Standby is finished initiate standby()
  if (fx_suggested_next == desk::fx::ALL_STOP) {
    INFO_AUTO("fx::Standby finished, initiating Desk::standby()\n");
    state = Stopping;

  } else if ((fx_suggested_next == desk::fx::STANDBY) && silent) {
    active_fx = std::make_unique<desk::Standby>(io_ctx);

  } else if ((fx_suggested_next == desk::fx::MAJOR_PEAK) && !silent) {
    active_fx = std::make_unique<desk::MajorPeak>(io_ctx);

  } else if (!silent && (frame_state.ready() || frame_state.future())) {
    active_fx = std::make_unique<desk::MajorPeak>(io_ctx);

  } else { // default to Standby
    active_fx = std::make_unique<desk::Standby>(io_ctx);
  }

  // note in log selected FX, if needed
  if (active_fx && !active_fx->match_name(fx_now)) {
    INFO_AUTO("FX {} -> {}\n", fx_now, active_fx->name());
  }
}

void Desk::handoff(uint8v &&packet, const uint8v &key) noexcept {
  if (racked) racked->handoff(std::forward<uint8v>(packet), key);
}

void Desk::resume() noexcept {
  static constexpr csv fn_id{"resume"};

  std::unique_lock lck(run_state_mtx, std::defer_lock);
  lck.lock();

  if (std::atomic_exchange(&state, Running) == Running) return;

  // the io_ctx may have been stopped since Desk is intended to be allocated
  // once per app run
  if (io_ctx.stopped()) io_ctx.restart();

  INFO_AUTO("requested, thread_count={}\n", thread_count);

  shutdown_latch = std::make_shared<std::latch>(thread_count);

  // submit work to io_ctx
  asio::post(io_ctx, [this]() {
    if (!racked) racked = std::make_unique<Racked>(master_clock, anchor.get());

    frame_loop();
  });

  for (auto n = 0; n < thread_count; n++) {
    std::jthread([this, n = n, shut_latch = shutdown_latch]() mutable {
      const auto thread_name = thread_util::set_name(TASK_NAME, n);

      // thread start syncronization not required
      INFO_THREAD_START();
      io_ctx.run();

      shut_latch->count_down();
      INFO_THREAD_STOP();
    }).detach();
  }

  INFO_AUTO("completed, thread_count={}\n", thread_count);
}

void Desk::spool(bool enable) noexcept {
  //// TODO: check this!
  if (racked) racked->spool(enable);
}

void Desk::standby() noexcept {
  static constexpr csv fn_id{"standby"};

  std::unique_lock lck(run_state_mtx, std::defer_lock);

  // if we can't aquire the lock then simply leave Desk running
  if (lck.try_lock() == false) return;

  // we're committed to stopping now, set the state
  if (std::atomic_exchange(&state, Stopped) == Stopped) return;

  INFO_AUTO("requested, io_ctx={}\n", io_ctx.stopped());

  io_ctx.stop();

  // shutdown supporting subsystems
  active_fx.reset();
  dmx_ctrl.reset();
  racked.reset();

  shutdown_latch->count_down();
  shutdown_latch.reset();

  INFO_AUTO("completed, io_ctx={}\n", io_ctx.stopped());
}
} // namespace pierre
