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
#include "base/conf/token.hpp"
#include "base/conf/toml.hpp"

#include <thread>

namespace pierre {

// NOTE: .cpp required to hide config.hpp

Dsp::Dsp() noexcept
    : work_guard{asio::make_work_guard(io_ctx)} //
{

  // notes:
  //  -concurrency_factor is specified in the config file as a percentage
  //  -to determine final thread count divide by 100
  conf::token ctoken(module_id);

  const auto concurrency_factor{ctoken.conf_val<int>("concurrency_factor"_tpath, 40)};
  const auto hw_concurrency{std::jthread::hardware_concurrency()};
  const auto threads{(hw_concurrency * concurrency_factor) / 100};

  INFO_INIT("sizeof={:>5} hw_concurrency={} thread_count={}\n", sizeof(Dsp), hw_concurrency,
            threads);

  // initialize FFT when the io_ctx is started
  asio::post(io_ctx, []() { FFT::init(); });

  // start the threads and run io_ctx
  for (uint32_t n = 0; n < threads; n++) {
    std::jthread([this]() { io_ctx.run(); }).detach();
  }
}

void Dsp::process(const frame_t frame, FFT &&left, FFT &&right) noexcept {
  frame->state = frame::DSP_IN_PROGRESS;

  asio::post(io_ctx, [this, frame = std::move(frame), left = std::move(left),
                      right = std::move(right)]() mutable {
    _process(std::move(frame), std::move(left), std::move(right));
  });
}

// we pass frame as const since we are not taking ownership
void Dsp::_process(const frame_t frame_ptr, FFT &&left, FFT &&right) noexcept {

  // minimize shared_ptr deferences while keeping it in scope
  auto &frame = *frame_ptr;

  // the caller sets the state to avoid a race condition with async processing
  frame.state = frame::DSP_IN_PROGRESS;

  // go async and move eveything required into the lambda
  // note: we capture a const shared_ptr to frame since we don't take ownership

  // the processing of frames will queue based on the number of workers available
  // (e.g. at start of play).  it is possible that one or more frames could be marked
  // as out of date.  we check the status of the frame between each step to avoid
  // unnecessary processing.

  if (frame.state == frame::DSP_IN_PROGRESS) {
    // the state hasn't changed, proceed with processing
    left.process();

    // check before starting the right channel (left required processing time)
    if (frame.state == frame::DSP_IN_PROGRESS) right.process();

    // check again since thr right channel also required processing time
    if (frame.state == frame::DSP_IN_PROGRESS) {
      left.find_peaks(frame.peaks, Peaks::CHANNEL::LEFT);

      if (frame.state == frame::DSP_IN_PROGRESS) {
        right.find_peaks(frame.peaks, Peaks::CHANNEL::RIGHT);
      }
    }

    frame.silent(frame.peaks.silence() && frame.peaks.silence());

    // atomically change the state to complete only if
    // it hasn't been changed elsewhere
    frame.state.store_if_equal(frame::DSP_IN_PROGRESS, frame::DSP_COMPLETE);
  }
}

} // namespace pierre