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
#include "av.hpp"
#include "base/io.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "config/config.hpp"
#include "fft.hpp"
#include "frame.hpp"
#include "peaks.hpp"
#include "types.hpp"

#include <latch>
#include <mutex>

namespace pierre {

/*
 * Many thanks to:
 * https://stackoverflow.com/questions/18862715/how-to-generate-the-aac-adts-elementary-stream-with-android-mediacodec
 *
 * Add ADTS header at the beginning of each and every AAC packet.
 * This is needed as MediaCodec encoder generates a packet of raw AAC data.
 *
 * NOTE: the payload size must include the ADTS header size
 */

// Digital Signal Analysis Async Threads
namespace dsp {
// order dependent
static io_context io_ctx;                   // digital signal analysis io_ctx
static steady_timer watchdog_timer(io_ctx); // watch for shutdown
static work_guard guard(io_ctx.get_executor());
static constexpr ccs thread_prefix{"FRAME DSP"};
static constexpr csv module_id{"DSP"};

// order independent
static Thread thread_main;
static Threads threads;
static std::stop_token stop_token;
static std::once_flag once_flag;

// initialize the thread pool for digital signal analysis
void init() {
  float dsp_hw_factor = Config::object("dsp")["concurrency_factor"];
  int thread_count = std::jthread::hardware_concurrency() * dsp_hw_factor;

  std::latch latch{thread_count};

  for (auto n = 0; n < thread_count; n++) {
    // notes:
    //  1. start DSP processing threads
    threads.emplace_back([=, &latch](std::stop_token token) {
      std::call_once(once_flag, [&token]() mutable {
        av::init();
        stop_token = token;
      });

      name_thread(thread_prefix, n);
      latch.count_down();
      io_ctx.run(); // dsp (frame) io_ctx
    });
  }

  watch_dog(); // watch for shutdown

  // caller thread waits until all threads are started
  latch.wait();
}

// perform digital signal analysis on a Frame
void process(frame_t frame) {
  asio::post(io_ctx, [=]() { frame->find_peaks(); });
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
      if (stop_token.stop_requested()) {
        io_ctx.stop();
      } else {
        watch_dog();
      }
    } else {
      __LOG0(LCOL01 " going out of scope reason={}\n", module_id, "WATCH_DOG", ec.message());
    }
  });
}

} // namespace dsp
} // namespace pierre