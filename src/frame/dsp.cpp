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

#include "frame/dsp.hpp"
#include "lcs/config.hpp"

namespace pierre {

// NOTE: .cpp required to hide config.hpp

Dsp::Dsp() noexcept : guard(asio::make_work_guard(io_ctx)) {

  static constexpr csv factor_path{"frame.dsp.concurrency_factor"};
  auto factor = config_val<double>(factor_path, 0.4);
  const int thread_count = std::jthread::hardware_concurrency() * factor;

  INFO_INIT("sizeof={:>5} thread_count={}\n", sizeof(Dsp), thread_count);

  auto latch = std::make_unique<std::latch>(thread_count);
  shutdown_latch = std::make_shared<std::latch>(thread_count);

  // as soon as the io_ctx starts precompute FFT windowing
  asio::post(io_ctx, []() { FFT::init(); });

  // start the configured number of workers (detached)
  // which will gracefully exit when work guard is reset
  for (auto n = 0; n < thread_count; n++) {

    // latch stays in scope until end of startup so a raw pointer is OK
    std::jthread([this, n = n, latch = latch.get()]() {
      auto const thread_name = thread_util::set_name(thread_prefix, n);

      latch->count_down();
      INFO_THREAD_START();
      io_ctx.run();

      shutdown_latch->count_down();
      INFO_THREAD_STOP();
    }).detach();
  }

  // caller thread waits until all workers are started
  latch->wait();
}

Dsp::~Dsp() noexcept {
  INFO_SHUTDOWN_REQUESTED();

  guard.reset();
  shutdown_latch->wait(); // caller waits for all workers to finish

  INFO_SHUTDOWN_COMPLETE();
}

void Dsp::process(const frame_t frame, FFT &&left, FFT &&right) noexcept {
  frame->state = frame::DSP_IN_PROGRESS;

  asio::post(io_ctx, [this, frame = std::move(frame), left = std::move(left),
                      right = std::move(right)]() mutable {
    _process(std::move(frame), std::move(left), std::move(right));
  });
}

void Dsp::_process(const frame_t frame, FFT &&left, FFT &&right) noexcept {

  // the caller sets the state to avoid a race condition with async processing
  frame->state = frame::DSP_IN_PROGRESS;

  // go async and move eveything required into the lambda
  // note: we capture a const shared_ptr to frame since we don't take ownership

  // the processing of frames will queue based on the number of workers available
  // (e.g. at start of play).  it is possible that one or more frames could be marked
  // as out of date.  we check the status of the frame between each step to avoid
  // unnecessary processing.

  if (frame->state == frame::DSP_IN_PROGRESS) {
    // the state hasn't changed, proceed with processing
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

    // atomically change the state to complete only if
    // it hasn't been changed elsewhere
    frame->state.store_if_equal(frame::DSP_IN_PROGRESS, frame::DSP_COMPLETE);
  }
}

} // namespace pierre