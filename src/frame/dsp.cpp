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

#include "dsp.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"
#include "config/config.hpp"
#include "fft.hpp"
#include "frame.hpp"
#include "peaks.hpp"

#include <algorithm>
#include <latch>
#include <ranges>
#include <vector>

namespace pierre {

//  Many thanks to:
//  https://stackoverflow.com/questions/18862715/how-to-generate-the-aac-adts-elementary-stream-with-android-mediacodec
//
//  Add ADTS header at the beginning of each and every AAC packet.
//  This is needed as MediaCodec encoder generates a packet of raw AAC data.
//
//  NOTE: the payload size must include the ADTS header size

// Digital Signal Analysis Async Threads
namespace dsp {
// order dependent
static io_context io_ctx;                   // digital signal analysis io_ctx
static steady_timer watchdog_timer(io_ctx); // watch for shutdown
static work_guard_t guard(io::make_work_guard(io_ctx));
static constexpr ccs thread_prefix{"FRAME DSP"};
static constexpr csv module_id{"DSP"};

// order independent
static Thread thread_main;
static Threads threads;
static stop_tokens tokens;

// initialize the thread pool for digital signal analysis
void init() {
  double factor = Config().table().at_path("frame.dsp.concurrency_factor"sv).value_or<double>(0.4);
  int thread_count = std::jthread::hardware_concurrency() * factor;

  std::latch latch{thread_count};

  for (auto n = 0; n < thread_count; n++) {
    static bool fft_initialized = false;
    // notes:
    //  1. start DSP processing threads
    threads.emplace_back([=, &latch](std::stop_token token) mutable {
      name_thread(thread_prefix, n);

      tokens.add(std::move(token));

      if (fft_initialized == false) {
        fft_initialized = true;
        asio::post(io_ctx, []() { FFT::init(); });
      }

      // allow DSP threads to start immediately so FFT is initialized
      latch.count_down();
      io_ctx.run();
    });
  }

  // caller thread waits until all threads are started
  latch.wait();

  watch_dog(); // watch for shutdown
}

// perform digital signal analysis on a Frame
void process(frame_t frame, FFT left, FFT right) {

  asio::post(io_ctx, [=, left = std::move(left), right = std::move(right)]() mutable {
    frame->state = frame::DSP_IN_PROGRESS;

    left.process();
    right.process();

    frame->peaks = std::make_tuple(left.find_peaks(), right.find_peaks());
    frame->state = frame::DSP_COMPLETE;
  });
}

// shutdown thread pool and wait for all threads to stop
void shutdown() {
  thread_main.request_stop();
  thread_main.join();
  ranges::for_each(threads, [](auto &t) { t.join(); });
}

// watch for thread stop request
void watch_dog() {
  // cancels any running timers
  [[maybe_unused]] auto canceled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([&](const error_code ec) {
    if (ec == errc::success) { // unless success, fall out of scape

      // check if any thread has received a stop request
      tokens.any_requested(io_ctx, &watch_dog);
    } else {
      INFO(module_id, "WATCH_DOG", "going out of scope reason={}\n", ec.message());
    }
  });
}

} // namespace dsp
} // namespace pierre