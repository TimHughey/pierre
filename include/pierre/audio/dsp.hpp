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

#include <thread>

#include "audio/fft.hpp"
#include "audio/samples.hpp"
#include "local/types.hpp"

namespace pierre {
namespace audio {

class Dsp : public Samples {

public:
  struct Config {
    uint samples = 1024;
    uint rate = 48000;

    struct {
      string_t path = "/dev/null";
    } log;
  };

public:
  Dsp(const Config &cfg);

  Dsp(const Dsp &) = delete;
  Dsp &operator=(const Dsp &) = delete;

  bool bass();
  const Peak majorPeak();

  std::shared_ptr<std::thread> run();
  void shutdown() { _shutdown = true; }

private:
  void stream();

private:
  bool _shutdown = false;

  Config _cfg;

  FFT _left = FFT(_cfg.samples, _cfg.rate);
  FFT _right = FFT(_cfg.samples, _cfg.rate);

  std::mutex _peaks_mtx;
  Peaks _peaks;
};

} // namespace audio
} // namespace pierre

#endif
