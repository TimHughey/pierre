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

#pragma once

#include "base/threads.hpp"
#include "base/uint8v.hpp"
#include "fft.hpp"
#include "frame.hpp"
#include "io/io.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"

#include <array>
#include <fmt/ostream.h>
#include <latch>
#include <memory>

namespace pierre {

class Dsp {
  friend Av;

protected:
  Dsp() noexcept : guard(asio::make_work_guard(io_ctx)) {

    static constexpr csv factor_path{"frame.dsp.concurrency_factor"};
    auto factor = config_val<double>(factor_path, 0.4);
    const int thread_count = std::jthread::hardware_concurrency() * factor;

    auto latch = std::make_unique<std::latch>(thread_count);
    shutdown_latch = std::make_shared<std::latch>(thread_count);

    // as soon as the io_ctx starts precompute FFT windowing
    asio::post(io_ctx, []() { FFT::init(); });

    // start the configured number of threads (detached)
    // the threads will finish when we reset the work guard
    for (auto n = 0; n < thread_count; n++) {

      // pass in a raw pointer to latch since the unique_ptr will
      // stay in-scope until all threads are started
      std::jthread([this, n = n, latch = latch.get()]() {
        auto const thread_name = name_thread(thread_prefix, n);

        INFO(module_id, "init", "thread {}\n", thread_name);

        latch->count_down();
        io_ctx.run();

        INFO(module_id, "shutdown", "thread {}\n", thread_name);
        shutdown_latch->count_down();
      }).detach();
    }

    // caller thread waits until all threads are started
    latch->wait();
  }

public:
  void process(const frame_t frame, FFT left, FFT right) noexcept {

    // the caller sets the state to avoid a race condition with async processing
    frame->state = frame::DSP_IN_PROGRESS;

    // go async and move eveything required into the lambda
    // note: we capture a const shared_ptr to frame since we don't take ownership
    asio::post(io_ctx,
               [this, frame = frame, left = std::move(left), right = std::move(right)]() mutable {
                 // due to limited threads processing of frames will queue (e.g. at start of play)
                 // it is possible that one or more of the queued frames could be marked as out of
                 // date by Racked. if the frame is anything other than decoded we skip peak
                 // detection.

                 if (frame->state == frame::DSP_IN_PROGRESS) {
                   // the frame state changed so we can proceed with the left channel
                   left.process();

                   // check before starting the right channel (left required processing time)
                   if (frame->state == frame::DSP_IN_PROGRESS) right.process();

                   // check again since thr right channel also required processing time
                   if (frame->state == frame::DSP_IN_PROGRESS) {
                     left.find_peaks(frame->peaks, Peaks::CHANNEL::LEFT);

                     if (frame->state == frame::DSP_IN_PROGRESS) {
                       right.find_peaks(frame->peaks, Peaks::CHANNEL::RIGHT);
                     }
                   }

                   // finding the peaks requires processing time so only change the state if
                   // still DSP_IN_PROGRESS
                   frame->state.store_if_equal(frame::DSP_IN_PROGRESS, frame::DSP_COMPLETE);
                 }
               });
  }

  void run() noexcept {}

  void shutdown() noexcept {
    static constexpr csv fn_id{"shutdown"};

    INFO(module_id, fn_id, "requested\n");

    guard.reset();

    shutdown_latch->wait(); // caller waits for all threads to finish

    INFO(module_id, fn_id, "io_ctx stopped={}\n", io_ctx.stopped());
  }

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;
  std::shared_ptr<std::latch> shutdown_latch;

  // order independent
  // Threads threads;

public:
  static constexpr csv thread_prefix{"dsp"};
  static constexpr csv module_id{"frame.dsp"};
};

} // namespace pierre
