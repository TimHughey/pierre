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

#include "frame/spooler.hpp"
#include "base/flush_request.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"
#include "frame/reel.hpp"

#include <algorithm>
#include <iterator>
#include <latch>
#include <optional>
#include <ranges>
#include <vector>

namespace pierre
{
  namespace shared
  {
    std::optional<Spooler> spooler;
  }

  // Spooler API and member functions
  void Spooler::accept(uint8v &packet)
  { // static
    shFrame frame = Frame::create(packet);

    __LOGX("{}\n", Frame::inspect(frame));

    // keep() returns true when:  a) not flushed;  b) deciphered OK
    if (frame->keep(flush_request))
    {
      asio::defer(strand_in, // guard with reels strand
                  [this, frame = std::move(frame)]()
                  {
                    shReel dst_reel = //
                        reels_in.empty() ? reels_in.emplace_back(Reel::create(strand_out))
                                         : reels_in.back();

                    dst_reel->addFrame(frame);
                  });
    }
  }

  void Spooler::clean()
  {
    asio::defer(strand_out, [this]() { // clean up empty reels
      auto reels_before = reels_out.size();
      auto erased = std::erase_if(reels_out, [](shReel reel)
                                  { return reel->empty(); });
      int remaining = static_cast<int>(reels_before) - static_cast<int>(erased);

      if (erased && (remaining <= 0))
      {
        __LOG0(LCOL01 " erased={} remaining={}\n", moduleID(), csv("REELS OUT"), erased, remaining);
      }
    });
  }

  void Spooler::flush(const FlushRequest &request)
  {
    // serialize Reels IN actions
    asio::post(strand_in, [=, this]
               { flush(request, reels_in); });

    // serialize Reels OUT actions
    asio::post(strand_out, [=, this]
               { flush(request, reels_out); });
  }

  // initialize the thread pool for digital signal analysis
  void Spooler::init()
  {
    if (!shared::spooler)
    {
      auto &spooler = shared::spooler.emplace();

      std::latch latch{THREAD_COUNT};

      // start spooler threads
      for (auto n = 0; n < THREAD_COUNT; n++)
      {
        // notes:
        //  1. start DSP processing threads
        spooler.threads.emplace_back([&]() mutable
                                     {
        name_thread(THREAD_NAME, n);
        latch.count_down();
        spooler.io_ctx.run(); });
      }

      spooler.watch_dog();

      // calling thread
      latch.wait();
    }

    // ensure Frame Digital Signal Analysis
    Frame::init();
  }

  shFrame Spooler::head_frame(AnchorLast &anchor)
  {

    // do we need a reel?
    requisition.ifNeeded();
    shFrame frame;

    if (!reels_out.empty())
    {
      if (auto &frames = reels_out.front()->frames(); !frames.empty())
      {
        frame = frames.front(); // get the head frame

        frames.pop_front();       // consume the head frame
        frame->state_now(anchor); // ensure the state and sync time are updated
      }
    }
    else
    {
      __LOG0(LCOL01 " no reels\n", moduleID(), "HEAD_FRAME");
    }

    return frame;
  }

  shFrame Spooler::next_frame(const Nanos lead_time, AnchorLast &anchor)
  {

    // do we need a reel?
    requisition.ifNeeded();
    shFrame frame;

    // two foor loops are required to find the next frame
    //  1. outer for-loop examines the out reels
    //  2. inner for-loop examines the frames in each out reel
    //  3. when a playable or future frame is found the frame (above) is set
    //  4. both inner and outer for-loop use frame as an exit condition
    //  5. use of ranges::find_if() was considered however this code
    //     is more readable and concise without the lambdas

    for (shReel &reel : reels_out)
    { // outer for-loop (reels)
      // grab a reference to the frames in the reel we are searching
      auto &frames = reel->frames();

      for (shFrame &pf : frames)
      { // inner for-loop (frames in reel)
        // calculate the frame state and sync_wait

        auto [valid, sync_wait, renderable] = pf->state_now(anchor, lead_time);

        if (valid && renderable)
        { // frame meets criteria for rendering
          frame = pf;
          break; // break out of inner for-loop (single reel)
        }
        else if (!valid)
        {
          // problem calculating sync wait, bail out
          break;
        }
      }

      reel->purge(); // always purge frames from searched reel

      if (frame) // found the next frame
        break;   //  break out of outer for-loop (reels)
    }

    clean(); // schedule spooler clean up

    return frame;
  }

  // shutdown thread pool and wait for all threads to stop
  void Spooler::shutdown()
  {
    auto &spooler = *shared::spooler;

    spooler.threads.front().request_stop();

    ranges::for_each(spooler.threads, [](auto &t)
                     { t.join(); });

    shared::spooler.reset(); // deallocate
  }

  // watch for thread stop request
  void Spooler::watch_dog()
  {
    // cancels any running timers
    [[maybe_unused]] auto canceled = watchdog_timer.expires_after(250ms);

    watchdog_timer.async_wait([this](const error_code ec)
                              {
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
    } });
  }

  // misc debug, stats
  const string Spooler::inspect() const
  {
    const auto &INDENT = __LOG_MODULE_ID_INDENT;

    string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{:<12} load={:<3} unload={:<3}\n", csv("REEL"), reels_in.size(),
                   reels_out.size());

    ranges::for_each(reels_in, [&](shReel reel)
                     { fmt::format_to(w, "{} {:<12} {}\n", INDENT, reel->moduleID(), Reel::inspect(reel)); });

    ranges::for_each(reels_out, [&](shReel reel)
                     { fmt::format_to(w, "{} {:<12} {}\n", INDENT, reel->moduleID(), Reel::inspect(reel)); });

    return msg;
  }

} // namespace pierre
