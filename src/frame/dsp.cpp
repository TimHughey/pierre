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

#include <boost/asio/dispatch.hpp>

namespace pierre {

// NOTE: .cpp required to hide config.hpp

Dsp::Dsp() noexcept
    : concurrency_factor{config_val<Dsp, uint32_t>("concurrency_factor"sv, 4)},
      thread_count{std::jthread::hardware_concurrency() * concurrency_factor / 10},
      thread_pool(thread_count),                //
      guard(asio::make_work_guard(thread_pool)) //
{
  INFO_INIT("sizeof={:>5} thread_count={}\n", sizeof(Dsp), thread_count);

  // as soon as the io_ctx starts precompute FFT windowing
  asio::dispatch(thread_pool, []() { FFT::init(); });
}

Dsp::~Dsp() noexcept {
  guard.reset();
  thread_pool.stop();
  thread_pool.join();
}

void Dsp::process(const frame_t frame, FFT &&left, FFT &&right) noexcept {
  frame->state = frame::DSP_IN_PROGRESS;

  asio::dispatch(thread_pool, [this, frame = std::move(frame), left = std::move(left),
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