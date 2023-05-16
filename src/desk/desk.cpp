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
#include <boost/asio/prepend.hpp>
#include <fmt/chrono.h>
#include <fmt/std.h>
#include <functional>
#include <set>
#include <thread>

namespace pierre {

// notes:
//  -must be defined in .cpp (incomplete types in .hpp)
//  -we use a separate io_ctx for desk and it's workers
Desk::Desk(MasterClock *master_clock) noexcept
    : tokc(module_id),
      // create and start racked early (frame rendering needs it)
      racked{std::make_unique<Racked>()},
      // cache the MasterClock pointer early (frame rendering needs it)
      master_clock(master_clock),
      // create the anchor early (frame rendering needs it)
      anchor(std::make_unique<Anchor>()),
      // create a strand to serialize render activities
      render_strand(asio::make_strand(io_ctx)),
      // create the frame timer
      loop_timer(render_strand, loop_clock::now()),
      // default Reel to be populated by next_reel
      reel(Reel::Starter),
      // create a synthetic for rendering when sender inactive
      syn_frame(frame::SILENCE) //
{
  INFO_INIT("sizeof={:>5} lead_time min={} nominal={}", //
            sizeof(Desk), pet::humanize(InputInfo::lead_time_min),
            pet::humanize(InputInfo::lead_time));

  // ensure flags are false
  resume_flag.clear();
  render_flag.clear();

  io_ctx.stop(); // needed by resume
  resume();
}

Desk::~Desk() noexcept {
  io_ctx.stop();

  INFO(module_id, "destruct", "io_ctx.stopped()={}", io_ctx.stopped());
}

////
//// API
///

void Desk::anchor_reset() noexcept {
  // protect changes to Anchor
  asio::post(render_strand, //
             asio::prepend([](Anchor *anc) { anc->reset(); }, anchor.get()));
}

void Desk::anchor_save(AnchorData &&ad) noexcept {
  // protect changes to Anchor
  asio::post(render_strand, [this, ad = std::move(ad)]() mutable { anchor->save(std::move(ad)); });
}

Frame &Desk::find_live_frame() noexcept {
  INFO_AUTO_CAT("find_live");

  static Frame sentinel(frame::SENTINEL);

  while (reel.size()) {
    auto &f = reel.front();

    f.state_now(*master_clock, *anchor);

    INFO_AUTO("{}", f);

    if (f.ready_or_future()) {
      return f;
    } else if (f.no_timing()) {
      return sentinel;
    } else {
      reel.pop_front();
    }
  }

  /* return std::find_if(reel.begin(), reel.end(), [this](Frame &f) {
     auto fstate = f.state_now(*master_clock, *anchor);

     if (fstate > frame::CAN_RENDER) return true;

     log_skipped_frame(f);

     return false;
   }); */

  return sentinel;
}

void Desk::flush(FlushInfo &&fi) noexcept {
  INFO_AUTO_CAT("flush");

  if (auto flushed = reel.flush(fi); flushed > 0) {
    INFO_AUTO("flushed={} avail={}", flushed, reel.size_cached());
    if (reel.empty()) reel = Reel(Reel::Transfer);
  }

  racked->flush(std::move(fi));

  /*asio::post(render_strand, asio::prepend(
                                [this](FlushInfo fi) {
                                  // perform the flush on our active reel first (if not empty)
                                  // before handing of request to Racked
                                  INFO_AUTO("{}", fi);

                                  if (!reel.empty()) fi(reel, frame::FLUSHED);
                                  if (racked) racked->flush(std::move(fi));
                                },
                                std::move(fi))); */
}

void Desk::flush_all() noexcept {
  if (racked) racked->flush_all();
}

void Desk::frame_live(Desk::frame_rr &frr) noexcept {
  INFO_AUTO_CAT("frame_live");

  if (frr.abort()) return;

  auto &found = find_live_frame();
  INFO_AUTO("{}", found);

  render(found, frr);

  if (frr.ok()) {
    // update the frr with the essential frame info
    // and record the frame's state
    frr.update(found);
  }

  /*if (it == reel.end()) {
    frr.need_clean = true;
    frr.sync_wait = 0ns;

    INFO_AUTO("no live frame, need_clean={}", frr.need_clean);
  } else {
    // got a frame
    INFO_AUTO("found {}", *it);

    it->record_state();
    render(*it, frr); // need to handle fx

    it->record_sync_wait();

    if (*it <= frame::READY) {
      it->mark_rendered();
      frr.need_clean = true;
    }
  }*/
}

void Desk::frame_syn(Desk::frame_rr &frr) noexcept {
  INFO_AUTO_CAT("render_sf");

  syn_frame = frame::READY;
  render(syn_frame, frr);

  if (frr.ok()) frr.update(syn_frame);
}

bool Desk::fx_finished() const noexcept { return (bool)active_fx ? active_fx->completed() : true; }

void Desk::fx_select(Frame &frame) noexcept {
  namespace fx = desk::fx;
  INFO_AUTO_CAT("fx_select");

  // cache multiple use frame info
  const auto silent = frame.silent();

  const auto fx_now = active_fx ? active_fx->name() : fx::NONE;
  const auto fx_next = active_fx ? active_fx->suggested_fx_next() : fx::STANDBY;

  // when fx::Standby is finished initiate standby()
  if (fx_now == fx::NONE) { // default to Standby
    active_fx = std::make_unique<desk::Standby>(render_strand);
  } else if (silent && (fx_next == fx::STANDBY)) {
    active_fx = std::make_unique<desk::Standby>(render_strand);

  } else if (!silent && (frame.can_render())) {
    active_fx = std::make_unique<desk::MajorPeak>(render_strand);

  } else if (silent && (fx_next == fx::ALL_STOP)) {
    active_fx = std::make_unique<desk::AllStop>(render_strand);
  } else {
    active_fx = std::make_unique<desk::Standby>(render_strand);
  }

  // note in log selected FX, if needed
  if (!active_fx->match_name(fx_now)) INFO_AUTO("FX {} -> {}\n", fx_now, active_fx->name());
}

void Desk::handoff(uint8v &&packet, const uint8v &key) noexcept {
  if (racked) racked->handoff(std::forward<decltype(packet)>(packet), key);
}

void Desk::log_skipped_frame(const Frame &f) noexcept { INFO("skipped", "{}", f); }

void Desk::loop() noexcept {
  INFO_AUTO_CAT("loop");

  Elapsed e;

  frame_rr frr;

  if (render_flag.test()) {
    frame_live(frr);
  } else {
    frame_syn(frr);
  }

  next_reel(frr);

  // schedule next loop, if needed
  schedule_loop(frr);

  Stats::write(stats::RENDER_ELAPSED, e.as<Nanos>());
}

void Desk::next_reel([[maybe_unused]] Desk::frame_rr &frr) noexcept {
  INFO_AUTO_CAT("next_reel");

  if (render_flag.test()) {
    auto [erase, remain] = reel.clean();
    if (erase) INFO_AUTO("cleaned, erase={} remain={}", erase, remain);

    if (reel.empty() && (bool)racked) {
      reel = Reel(Reel::Transfer);
      INFO_AUTO("requesting next reel");
      racked->next_reel(reel);

      INFO_AUTO("got {}", reel);
    }
  }
}

bool Desk::render(Frame &frame, frame_rr &frr) noexcept {
  INFO_AUTO_CAT("render");

  INFO_AUTO("{}", frame);

  if (fx_finished()) fx_select(frame);

  frr.stop = shutdown_if_all_stop();

  if (frr.ok() && frame.ready()) {
    // the frame is ready for rendering
    desk::DataMsg msg(frame.sn(), frame.silent());

    // send the data message if rendered
    if (active_fx->render(frame.get_peaks(), msg)) {
      if ((bool)dmx_ctrl) dmx_ctrl->send_data_msg(std::move(msg));
    }

  } else {
    INFO_AUTO("NOT RENDERED {}", frame);
  }

  return frr.stop;
}

void Desk::resume() noexcept {
  INFO_AUTO_CAT("resume");

  auto is_resuming = !resume_flag.test_and_set();
  INFO_AUTO("invoked, stopped={} resuming={}", io_ctx.stopped(), is_resuming);

  // if we were already resuming don't attempt another
  if (is_resuming && io_ctx.stopped()) {
    resume_flag.test_and_set();

    if (threads.size()) threads_stop();

    INFO_AUTO("reset threads={}", threads.size());
    io_ctx.reset();

    // add work to io_ctx before starting threads
    asio::post(render_strand, [this]() {
      anchor->reset();

      // prevent duplicate reset of DmxCtrl at startup
      if (!(bool)dmx_ctrl) dmx_ctrl = std::make_unique<desk::DmxCtrl>();

      dmx_ctrl->resume();

      loop();
      resume_flag.clear();
    });

    threads_start();
  }
}

void Desk::schedule_loop(Desk::frame_rr &frr) noexcept {
  INFO_AUTO_CAT("schedule_loop");

  if (frr.abort()) {
    INFO_AUTO("not scheduling, stop={} rendered={}", frr.stop, frr.rendered);
    return;
  }

  if (!frr.rendered) {
    asio::post(render_strand, [this]() { loop(); });
  } else {
    Frame::sync_wait(*master_clock, *anchor, frr);

    loop_timer.expires_after(frr.sync_wait);
    loop_timer.async_wait([this](const error_code &ec) {
      if (!ec) loop();
    });
  }
}

void Desk::set_render(bool enable) noexcept {
  INFO_AUTO_CAT("set_render");

  if (enable) {
    auto prev = render_flag.test_and_set();

    INFO_AUTO("render_flag={} prev={} enable={}", render_flag.test(), prev, enable);

  } else {
    INFO_AUTO("render_flag={} enable={}", render_flag.test(), enable);
  }
}

bool Desk::shutdown_if_all_stop() noexcept {
  INFO_AUTO_CAT("shutdown_all");

  auto rc = (bool)active_fx && active_fx->match_name(desk::fx::ALL_STOP);

  if (rc) {
    // halt the frame timer
    [[maybe_unused]] error_code ec;
    loop_timer.cancel(ec);

    INFO_AUTO("active_fx={} io_ctx.stopped={}", active_fx->name(), io_ctx.stopped());

    // reset Desk state
    anchor->reset();
    reel = Reel(Reel::Starter);
    active_fx.reset();

    // disconnect from ruth
    dmx_ctrl.reset();

    threads_stop();
  }

  return rc;
}

void Desk::threads_start() noexcept {
  INFO_AUTO_CAT("threads_start");

  auto n_thr = tokc.val<int>("threads"sv, 2);

  auto latch = std::make_shared<std::latch>(n_thr + 1);
  threads = std::vector<std::jthread>(n_thr);
  INFO_AUTO("starting threads={}", std::ssize(threads));

  for (auto &t : threads) {
    const auto tname = fmt::format("pierre_desk{}", n_thr--);

    t = std::jthread([this, tname = std::move(tname), latch = latch]() {
      INFO_AUTO("starting {}", tname);

      pthread_setname_np(pthread_self(), tname.c_str());

      latch->arrive_and_wait();

      INFO_AUTO("io_ctx.run() tname={}", tname);

      io_ctx.run();
    });
  }

  latch->arrive_and_wait();

  INFO_AUTO("started threads={}", threads.size());
}

void Desk::threads_stop() noexcept {
  INFO_AUTO_CAT("threads_stop");

  std::latch latch(threads.size() + 1);

  auto st = std::jthread([this, latch = &latch]() { // ensure any previous threads are released
    io_ctx.stop();

    INFO_AUTO("joining threads={} stopped={}", threads.size(), io_ctx.stopped());

    std::erase_if(threads, [=](auto &t) mutable {
      if (t.joinable()) {
        INFO_AUTO("joining tid={}", t.get_id());
        t.join();

        latch->count_down();
      }

      return true;
    });
  });

  st.detach();
  INFO_AUTO("waiting for threads to stop");
  latch.arrive_and_wait();
  INFO_AUTO("joining complete");
}

} // namespace pierre
