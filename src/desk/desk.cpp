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
#include "frame/flusher.hpp"
#include "frame/frame.hpp"
#include "frame/master_clock.hpp"
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
      // cache the MasterClock pointer early (frame rendering needs it)
      master_clock(master_clock),
      // create the anchor early (frame rendering needs it)
      anchor(std::make_unique<Anchor>()),
      // create a strand to serialize render activities
      render_strand(asio::make_strand(io_ctx)),
      //
      flush_strand(asio::make_strand(io_ctx)),
      // create the frame timer
      loop_timer(render_strand, loop_clock::now()),
      // "A" reel (from sender)"
      reel(Reel::Ready),
      // the flusher
      flusher() //
{
  INFO_INIT("sizeof={:>5} fps={}", sizeof(Desk), InputInfo::fps);
  Frame::init();

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

  flusher.acquire();
  anchor->reset();
  flusher.release();
}

void Desk::anchor_save(AnchorData &&ad) noexcept {

  flusher.acquire();
  anchor->save(std::move(ad));
  flusher.release();
}

void Desk::flush(FlushInfo &&fi) noexcept {
  INFO_AUTO_CAT("flush");

  // safely moves FlushInfo into flush_request and sets it active
  // accept the FlushInfo *before* posting to flush_strand
  flusher.accept(std::move(fi));

  asio::post(flush_strand, [this]() {
    if (auto flushed = reel.flush(flusher); flushed > 0) {
      INFO_AUTO("{} flushed={} avail={}", reel.serial_num<string>(), flushed, reel.size_cached());
    }

    flusher.done();
  });
}

bool Desk::frame_render(frame_rr &frr) noexcept {
  INFO_AUTO_CAT("frame_render");

  INFO_AUTO("{}", frr());

  if (fx_finished()) fx_select(frr());

  frr.stop = shutdown_if_all_stop();

  if (frr.ok() && frr().ready()) {
    // the frame is ready for rendering
    desk::DataMsg msg(frr().sn(), frr().silent());

    // send the data message if rendered
    if (active_fx->render(frr().get_peaks(), msg)) {
      if ((bool)dmx_ctrl) dmx_ctrl->send_data_msg(std::move(msg));
    }

  } else {
    INFO_AUTO("NOT RENDERED {}", frr());
  }

  return frr.stop;
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
  INFO_AUTO_CAT("handoff");

  if (packet.empty()) return; // quietly ignore empty packets

  // perform the heavy lifting of decipher, decode and dsp in the caller's thread
  Frame frame(packet);
  frame.process(std::move(packet), key);

  reel.push_back(std::move(frame));
}

void Desk::loop() noexcept {
  INFO_AUTO_CAT("loop");

  Elapsed e;
  frame_rr frr(Frame(frame::SILENCE));

  if (render_flag.test()) {
    if (flusher.acquire()) {

      if (reel.size()) {
        auto clk_info = master_clock->info();
        reel.frame_next(anchor->get_data(clk_info), flusher, frr());
        frame_render(frr);

      } else {
        frame_render(frr);
      }

      flusher.release();

    } else {
      INFO_AUTO("failed to acquire flusher");
    }

  } else {
    frame_render(frr);
  }

  if (frr.abort()) return;

  if (frr().sync_wait() == 0ns) {
    asio::post(render_strand, [this]() {
      reel.clean();
      loop();
    });
  } else {
    loop_timer.expires_after(frr().sync_wait());

    asio::post(render_strand, [this]() { reel.clean(); });
    loop_timer.async_wait([this](const error_code &ec) {
      if (!ec) loop();
    });
  }

  frr.finish();

  Stats::write(stats::RENDER_ELAPSED, e.as<Nanos>());
}

void Desk::resume() noexcept {
  INFO_AUTO_CAT("resume");

  auto is_resuming = !resume_flag.test_and_set();
  INFO_AUTO("stopped={} resuming={}", io_ctx.stopped(), is_resuming);

  // if we were already resuming don't attempt another
  if (is_resuming && io_ctx.stopped()) {
    resume_flag.test_and_set();

    if (threads.size()) {
      threads_stop();
      INFO_AUTO("reset threads={}", threads.size());
    }

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

void Desk::set_rendering(bool enable) noexcept {
  INFO_AUTO_CAT("set_rendering");

  auto prev = render_flag.test();

  if (enable) {
    prev = render_flag.test_and_set();
  } else {
    render_flag.clear();
  }

  INFO_AUTO("render_flag={} prev={} enable={}", render_flag.test(), prev, enable);
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
    reel.reset();
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
