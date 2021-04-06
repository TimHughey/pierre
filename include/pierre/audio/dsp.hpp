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

#ifndef _pierre_audio_dsp_hpp
#define _pierre_audio_dsp_hpp

#include <string>
#include <thread>

#include "audio/fft.hpp"
#include "audio/samples.hpp"
#include "core/config.hpp"

namespace pierre {
namespace audio {

class Dsp : public Samples {
public:
  using mutex = std::mutex;
  using string = std::string;
  using thread = std::thread;
  using spThread = std::shared_ptr<thread>;

public:
  Dsp(core::Config &cfg);

  Dsp(const Dsp &) = delete;
  Dsp &operator=(const Dsp &) = delete;

  spPeaks peaks();
  spThread run();

private:
  void doFFT(FFT &fft);
  void stream();

private:
  FFT _fft_left;

  mutex _peaks_mtx;
  spPeaks _peaks;
};

} // namespace audio
} // namespace pierre

#endif
