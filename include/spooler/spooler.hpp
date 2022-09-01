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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#pragma once

#include "base/flush_request.hpp"
#include "base/pe_time.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "frame/frame.hpp"
#include "io/io.hpp"
#include "spooler/reel.hpp"
#include "spooler/requisition.hpp"

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <ranges>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <vector>

namespace pierre {

class Spooler;

Spooler *ispooler();

class Spooler {
private:
  Spooler()
      : strand_in(io_ctx),            // guard Reels reels_in
        strand_out(io_ctx),           // guard Reels reels_out
        watchdog_timer(io_ctx),       // watch for stop request
        guard(io_ctx.get_executor()), // provide idle work for io_ctx
        requisition(strand_in, reels_in, strand_out, reels_out) {}

public:
  // instance API
  static void init();
  static void shutdown();

  // general API
  void accept(uint8v &packet);

  template <typename CompletionToken>
  auto async_next_frame(const Nanos &lead_time, io_context &io_ctx, CompletionToken &&token) {

    auto initiation = [](auto &&comp_handler, const Nanos &lead_time, io_context &io_ctx) {
      struct intermediate_handler {
        const Nanos lead_time;         // the lead time for retrieving the next frame
        io_context &io_ctx;            // strand for handoff handler
        shFrame frame;                 // the inflight frame
        enum { deque, handoff } state; // current state
        typename std::decay<decltype(comp_handler)>::type handler; // user supplied handler

        void operator()() // async::post() requires a NullaryToken
        {
          __LOGX(LCOL01 " state={}\n", "SPOOLER", "ASYNC_NEXT_FRAME", state);

          switch (state) {
          case deque:
            // this case is running in spooler strand_out
            state = handoff; // the next state
            frame = ispooler()->next_frame(lead_time);

            asio::post(io_ctx, std::move(*this));
            return; // more async work to do

          case handoff:
            // we've reached the end of the async::post() chain, call user
            // supplied handler
            handler(std::move(frame));
          }
        }
      }; // end intermediate struct

      // initiate post operation via intermediate handler
      asio::post(                                           // execute async
          ispooler()->strand_out,                           // strand to use
          intermediate_handler{lead_time,                   // the lead time
                               io_ctx,                      // pass along the dst strand
                               shFrame(),                   // uninitialized frame
                               intermediate_handler::deque, // initial state
                               std::forward<decltype(comp_handler)>(comp_handler)});
    }; // end initiation function

    return asio::async_initiate<CompletionToken, void(shFrame frame)>(
        initiation,      // initiation function object
        token,           // user supplied callback
        lead_time,       // reel containing the frame
        std::ref(io_ctx) // wrap non-const args to prevent incorrect decay-copies
    );
  }

  void flush(const FlushRequest &flush_request);

  // public misc debug
  const string inspect() const;
  const csv moduleID() const { return csv(module_id); }

private:
  void clean();
  void flush(const FlushRequest &request, Reels &reels) {
    // create new reels containing the desired seq_nums
    Reels reels_keep;

    // transfer un-flushed rtp_packets to new reels
    ranges::for_each(reels, [&](shReel reel) {
      if (auto keep = reel->flush(request); keep == true) { // reel has frames
        __LOG0("{:<18} FLUSH KEEP {}\n", reel->moduleID(), Reel::inspect(reel));

        reels_keep.emplace_back(reel);
      }
    });

    // swap kept reels
    std::swap(reels_keep, reels);
  }

  shFrame next_frame(const Nanos lead_time);

  void watch_dog();

  // misc logging
  void async_log() {
    asio::post(strand_in, [&] { sync_log(); });
  }

  void sync_log() { __LOG0(LCOL01 " {}\n", module_id, "INSPECT", inspect()); }

private:
  // order dependent (constructor initialized)
  io_context io_ctx; // spooler context (for containers)
  strand strand_in;
  strand strand_out;
  steady_timer watchdog_timer; // watch for shutdown
  work_guard guard;
  Reels reels_in;
  Reels reels_out;
  Requisition requisition; // guard by strand_out

  // threads
  Threads threads;
  std::stop_token stop_token;

  // order independent
  FlushRequest flush_request; // flush request (applied to reels_in and reels_out)

  // static constants
  static constexpr auto *THREAD_NAME = "Spooler";
  static constexpr auto THREAD_COUNT = 2;
  static constexpr csv module_id{"SPOOLER"};
};

} // namespace pierre
