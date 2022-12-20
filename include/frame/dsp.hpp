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

#include "base/io.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "fft.hpp"
#include "frame.hpp"

#include <latch>
#include <memory>

namespace pierre {

class Dsp : public std::enable_shared_from_this<Dsp> {
private:
  Dsp() noexcept : guard(io::make_work_guard(io_ctx)) {}
  auto ptr() noexcept { return shared_from_this(); }

public:
  static auto create() noexcept {

    auto self = std::shared_ptr<Dsp>(new Dsp());

    auto s = self->ptr();

    auto thread_count = cfg_thread_count();

    std::latch latch{thread_count};

    for (auto n = 0; n < thread_count; n++) {
      // notes:
      //  1. start DSP processing threads
      self->threads.emplace_back([n = n, s = s->ptr(), &latch]() mutable {
        name_thread(thread_prefix, n);

        latch.arrive_and_wait();
        s->io_ctx.run();
      });
    }

    FFT::init();

    // caller thread waits until all threads are started
    latch.wait();

    return self->ptr();
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

  static void shutdown() noexcept {
    auto s = self->ptr();
    auto &io_ctx = s->io_ctx; // get reference, we're moving s into lambda

    asio::post(io_ctx, [s = std::move(s)]() {
      // by moving s into this lambda we keep this object in-scope while
      // resetting the work guard.  any threads performing dsp processing
      // have their own shared pointer.  the io_ctx will fall through because
      // of lack of work and the thread will exit
      s->guard->reset();
    });

    self.reset(); // reset our self
  }

private:
  static int cfg_thread_count() noexcept { // static
    static toml::path factor_path{"frame.dsp.concurrency_factor"sv};

    double factor = Config().table().at_path(factor_path).value_or<double>(0.4);
    return std::jthread::hardware_concurrency() * factor;
  }

private:
  // order dependent
  io_context io_ctx;
  work_guard_t guard;

  // order independent
  static std::shared_ptr<Dsp> self;
  Threads threads;

public:
  static constexpr csv thread_prefix{"DSP"};
  static constexpr csv module_id{"DSP"};
};

} // namespace pierre
