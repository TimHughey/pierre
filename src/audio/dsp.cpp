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

#include <fstream>
#include <iostream>
#include <thread>

#include "audio/dsp.hpp"

using namespace std;
using namespace chrono;

namespace pierre {
namespace audio {
Dsp::Dsp(const Config &cfg) : _cfg(cfg) {}

bool Dsp::bass() {
  bool bass = false;

  std::lock_guard<std::mutex> lck(_peaks_mtx);

  for (const Peak &peak : _peaks) {
    if (!peak.magStrong()) {
      break;
    } else if ((peak.freq > 30.0) && (peak.freq <= 170.0)) {
      bass = true;
      break;
    }
  }

  return bass;
}

const Peak Dsp::majorPeak() {
  std::lock_guard<std::mutex> lck(_peaks_mtx);

  auto peak = _left.majorPeak(_peaks);

  return std::move(peak);
}

shared_ptr<thread> Dsp::run() {
  auto t = make_shared<thread>([this]() { this->stream(); });
  return t;
}

void Dsp::stream() {
  ofstream fft_log(_cfg.log.path, ofstream::out | ofstream::trunc);

  // fft requires collecting _fft_samples via these iterators before processing
  auto &left_real = _left.real();
  left_real.assign(left_real.size(), 0);
  auto left_pos = left_real.begin();
  auto left_end = left_real.cend();

  auto &right_real = _right.real();
  auto right_pos = right_real.begin();

  while (_shutdown == false) {
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
        _left.process();

        {
          lock_guard<std::mutex> lck(_peaks_mtx);
          _left.findPeaks(_peaks);
        }

        // skip FFT for _right to improve performance
        // _right.process(_peaks)

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
