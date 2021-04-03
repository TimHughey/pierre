/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "audio/dsp.hpp"
#include "core/state.hpp"
#include "misc/elapsed.hpp"

using namespace std;
using namespace chrono;

namespace pierre {
using State = core::State;

namespace audio {
Dsp::Dsp(const Config &cfg) : _cfg(cfg) { _peaks = make_shared<Peaks>(); }

spPeaks Dsp::peaks() {
  lock_guard<mutex> lck(_peaks_mtx);

  return _peaks;
}

shared_ptr<thread> Dsp::run() {
  auto t = make_shared<thread>([this]() { this->stream(); });
  return t;
}

void Dsp::stream() {
  // fft requires collecting _fft_samples via these iterators before processing
  auto &left_real = _left.real();
  left_real.assign(left_real.size(), 0);
  auto left_pos = left_real.begin();
  auto left_end = left_real.cend();

  auto &right_real = _right.real();
  auto right_pos = right_real.begin();

  while (State::running()) {
    const auto entry = pop();         // actual queue entry
    const auto &samples = entry->raw; // raw samples

    // iterate through all the samples populating left and right FFT
    // with floats.  when FFT real is fully populated calculate, find
    // peaks the reset the iterators to continue populating any
    // remaining samples.

    auto sample = samples.cbegin();
    do {
      if (left_pos == left_end) {
        // enough samples have been collected, do FFT and keep a copy
        // of the Peaks locally
        // auto left_thr = make_unique<thread>([this]() {
        _left.process();

        {
          lock_guard<mutex> lck(_peaks_mtx);
          _peaks = std::make_shared<Peaks>();
          _left.findPeaks(_peaks);
        }

        // });

        // auto right_thr = make_unique<thread>([this]() { _right.process();
        // });

        // left_thr->join();
        // right_thr->join();

        // reset left and right real positions to continue loading
        left_pos = left_real.begin();
        left_real.assign(left_real.size(), 0);
        right_pos = right_real.begin();
      }

      // consume two samples
      *left_pos++ = float(*sample++);
      *right_pos++ = float(*sample++);
    } while (sample != samples.cend());
  }
}

} // namespace audio
} // namespace pierre
