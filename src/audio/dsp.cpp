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
Dsp::Dsp(core::Config &cfg)
    : _fft_left(cfg.dsp("fft"sv)->get("samples")->value_or(1024),
                cfg.dsp("fft"sv)->get("rate")->value_or(48000))

{
  _peaks = make_shared<Peaks>();
}

void Dsp::doFFT(FFT &fft) {
  // calculate the FFT and find peaks
  fft.process();
  {
    lock_guard<mutex> lck(_peaks_mtx);
    _peaks = std::make_shared<Peaks>();
    fft.findPeaks(_peaks);
  }
}

spPeaks Dsp::peaks() {
  lock_guard<mutex> lck(_peaks_mtx);

  return _peaks;
}

Dsp::spThread Dsp::run() {
  auto t = make_shared<thread>([this]() { this->stream(); });
  return t;
}

void Dsp::stream() {
  auto &left = _fft_left.real();
  auto left_pos = left.begin();

  while (State::running()) {
    const auto entry = pop();         // actual queue entry
    const auto &samples = entry->raw; // raw samples

    // collect samples into the FFT
    // when enough samples have been collected calculate and find the peaks
    for (const auto &sample : samples) {
      if (left_pos == left.cend()) {
        doFFT(_fft_left);

        // reset left_pos iterator to where the next sample is put
        left_pos = left.begin();
      }

      // copy the sample to the FFT as a float
      *left_pos++ = static_cast<float>(sample);
    }
  }
}

} // namespace audio
} // namespace pierre
