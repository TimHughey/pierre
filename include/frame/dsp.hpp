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

#include "io/io.hpp"
#include "base/threads.hpp"
#include "base/uint8v.hpp"
#include "fft.hpp"
#include "frame.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"

#include <fmt/ostream.h>
#include <latch>
#include <memory>

namespace pierre {

class Dsp : public std::enable_shared_from_this<Dsp> {
private:
  Dsp() noexcept : guard(asio::make_work_guard(io_ctx)) {}
  auto ptr() noexcept { return shared_from_this(); }

public:
  static auto create() noexcept {
    auto self = std::shared_ptr<Dsp>(new Dsp());

    static constexpr csv factor_path{"frame.dsp.concurrency_factor"};
    auto factor = config_val<double>(factor_path, 0.4);
    const int thread_count = std::jthread::hardware_concurrency() * factor;

    auto latch = std::make_shared<std::latch>(thread_count);

    for (auto n = 0; n < thread_count; n++) {
      // notes:
      //  1. start DSP processing threads
      self->threads.emplace_back([n = n, s = self->ptr(), latch = latch]() mutable {
        name_thread(thread_prefix, n);

        latch->arrive_and_wait();
        latch.reset();
        s->io_ctx.run();
      });
    }

    FFT::init();

    // caller thread waits until all threads are started
    latch->wait();

    return self;
  }

  void process(frame_t frame, FFT left, FFT right) noexcept {
    auto s = ptr();

    // the caller sets the state to avoid a race condition with async processing
    frame->state = frame::DSP_IN_PROGRESS;

    auto &io_ctx = s->io_ctx;

    // move eveything required into the lambda
    asio::post(io_ctx, [s = std::move(s), frame = std::move(frame), left = std::move(left),
                        right = std::move(right)]() mutable {
      // due to limited threads processing of frames will queue (e.g. at start of play)
      // it is possible that one or more of the queued frames could be marked as out of date
      // by Racked. if the frame is anything other than decoded we skip peak detection.

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

        // finding the peaks requires processing so only change the state if
        // it is still DSP_IN_PROGRESS
        frame->state.store_if_equal(frame::DSP_IN_PROGRESS, frame::DSP_COMPLETE);
      }
    });
  }

  void shutdown() noexcept {
    static constexpr csv fn_id{"shutdown"};

    INFO(module_id, fn_id, "initiated, io_ctx.stopped()={}\n", io_ctx.stopped());

    guard.reset();
    io_ctx.stop();

    std::for_each(threads.begin(), threads.end(), [](auto &t) {
      INFO(module_id, fn_id, "joining thread={}\n", t.get_id());

      t.join();
    });

    threads.clear();

    INFO(module_id, fn_id, "io_ctx.stopped()={}\n", io_ctx.stopped());
  }

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;

  // order independent
  Threads threads;

public:
  static constexpr csv thread_prefix{"dsp"};
  static constexpr csv module_id{"frame.dsp"};
};

} // namespace pierre
