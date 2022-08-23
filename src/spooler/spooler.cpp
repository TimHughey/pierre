//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#include "spooler/spooler.hpp"
#include "base/flush_request.hpp"
#include "base/pe_time.hpp"
#include "base/threads.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"
#include "spooler/reel.hpp"

#include <algorithm>
#include <iterator>
#include <latch>
#include <optional>
#include <ranges>
#include <vector>

namespace pierre {

namespace spool { // instance
std::unique_ptr<Spooler> i;
} // namespace spool

Spooler *ispooler() { return spool::i.get(); }

// Spooler API and member functions
void Spooler::accept(uint8v &packet) { // static
  shFrame frame = Frame::create(packet);

  __LOGX("{}\n", Frame::inspect(frame));

  // keep() returns true when:  a) not flushed;  b) deciphered OK
  if (frame->keep(flush_request)) {
    asio::defer(strand_in, // guard with reels strand
                [this, frame = std::move(frame)]() {
                  shReel dst_reel = //
                      reels_in.empty() ? reels_in.emplace_back(Reel::create(strand_out))
                                       : reels_in.back();

                  dst_reel->addFrame(frame);
                });
  }
}

void Spooler::clean() {
  asio::defer(strand_out, [this]() { // clean up empty reels
    auto reels_before = reels_out.size();
    auto erased = std::erase_if(reels_out, [](shReel reel) { return reel->empty(); });
    int remaining = static_cast<int>(reels_before) - static_cast<int>(erased);

    if (erased && (remaining <= 0)) {
      __LOG0(LCOL01 " erased={} remaining={}\n", moduleID(), csv("REELS OUT"), erased, remaining);
    }
  });
}

void Spooler::flush(const FlushRequest &request) {
  // serialize Reels IN actions
  asio::post(strand_in, [=, this] { flush(request, reels_in); });

  // serialize Reels OUT actions
  asio::post(strand_out, [=, this] { flush(request, reels_out); });
}

// initialize the thread pool for digital signal analysis
void Spooler::init() {
  if (auto self = spool::i.get(); !self) {
    self = new Spooler();
    spool::i = std::move(std::unique_ptr<Spooler>(self));

    std::latch latch{THREAD_COUNT};

    // start spooler threads
    for (auto n = 0; n < THREAD_COUNT; n++) {
      // notes:
      //  1. start DSP processing threads
      self->threads.emplace_back([=, &latch] {
        name_thread(THREAD_NAME, n);
        latch.count_down();
        self->io_ctx.run();
      });
    }

    self->watch_dog();

    // calling thread
    latch.wait();
  }

  // ensure Frame Digital Signal Analysis
  Frame::init();
}

std::future<shFrame> Spooler::next_frame(const Nanos lead_time, const Nanos lag) {
  std::promise<shFrame> promise;
  auto future = promise.get_future();

  // do we need a reel?
  requisition.ifNeeded();

  asio::post(strand_out, [=, promise = std::move(promise), this]() mutable {
    shFrame frame;

    // two foor loops are required to find the next frame
    //  1. outer for-loop examines the out reels
    //  2. inner for-loop examines the frames in each out reel
    //  3. when a playable frame is found the frame (above) is set
    //  4. both inner and outer for-loop use frame as an exit condition
    //  5. use of ranges::find_if() was considered however the final code
    //     is more readable and concise without the lambdas

    for (shReel &reel : reels_out) { // outer for-loop (reels)

      // grab a reference to the frames in the reel we are searching
      auto &frames = reel->frames();

      for (shFrame &pf : frames) { // inner for-loop (frames in reel)

        if (pf->next(lead_time, lag)) { // possible frame is the next frame
          frame = pf;
          break; // break out of inner for-loop
        }
      }

      reel->purge(); // always purge played (or outdated) frames

      // found the next frame
      if (frame) {
        break; //  break out of outer for-loop
      }
    }

    // either frame was found or all reels were searched, set the promise value
    promise.set_value(frame);
    clean(); // schedule spooler clean up
  });

  return future;
}

/*
std::future<shFrame> Spooler::next_frame(const Nanos lead_time) {
  std::promise<shFrame> promise;
  auto future = promise.get_future();

  asio::post(strand_out, [lead_time, promise = std::move(promise), this]() mutable {
    // do we need a reel?
    requisition.ifNeeded();

    shFrame frame;

    // two finds could be necessary.
    //  1. the outer find_if() examines each reel
    //  2. the inner find_if() examines each frame in the reel
    //  3. if the inner find_if() finds a playable frame then frame is set

    // outer (returns the reel that contains the frame)
    auto reel = ranges::find_if(reels_out, [&](auto reel) {
      // grab a reference to the frames in the reel
      const Frames &frames = reel->frames();
      // inner
      auto nf = ranges::find_if(frames, [&](shFrame nf) {
        // call Frame::next_frame():
        //  1.  true if frame is playable
        //  2. otherwise frame
        // set the playable frame to captured frame if playable
        if (nf->next(lead_time)) {
          frame = nf;
          return true; // signal the frame was found to outer find_if()
        }

        return false; // signal outer find_if() no playable frame found
      });

      // bacl to outer find
      reel->purge(); // reel may have played frames, purge them

      // back to outer find_if(), check if frame was found
      return nf != frames.end() ? true : false;
    });

    // inner and outer find are complete, was a frame found?
    promise.set_value(reel != reels_out.end() ? frame : shFrame());

    clean(); // clean the spooler
  });

  return future;
}

*/

// shutdown thread pool and wait for all threads to stop
void Spooler::shutdown() {
  auto self = ispooler();

  self->threads.front().request_stop();

  ranges::for_each(self->threads, [](auto &t) { t.join(); });

  spool::i.reset(); // deallocate
}

// watch for thread stop request
void Spooler::watch_dog() {
  // cancels any running timers
  [[maybe_unused]] auto canceled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([this](const error_code ec) {
    if (ec == errc::success) { // unless success, fall out of scape

      // check if any thread has received a stop request
      if (stop_token.stop_requested()) {
        io_ctx.stop();
      } else {
        watch_dog();
      }
    } else {
      __LOG0(LCOL01 " going out of scope reason={}\n", //
             moduleID(), csv("WATCH_DOG"), ec.message());
    }
  });
}

// misc debug, stats
const string Spooler::inspect() const {
  const auto &INDENT = __LOG_MODULE_ID_INDENT;

  string msg;
  auto w = std::back_inserter(msg);

  fmt::format_to(w, "{:<12} load={:<3} unload={:<3}\n", csv("REEL"), reels_in.size(),
                 reels_out.size());

  ranges::for_each(reels_in, [&](shReel reel) {
    fmt::format_to(w, "{} {:<12} {}\n", INDENT, reel->moduleID(), Reel::inspect(reel));
  });

  ranges::for_each(reels_out, [&](shReel reel) {
    fmt::format_to(w, "{} {:<12} {}\n", INDENT, reel->moduleID(), Reel::inspect(reel));
  });

  return msg;
}

} // namespace pierre
