/*
Pierre - FFT Library specialized for Wiss Landing custom light show

Copyright (C) 2010 Didier Longueville
Copyright (C) 2014 Enrique Condes
Copyright (C) 2020 Bim Overbohm
Copyright (C) 2021 Tim Hughey

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

#pragma once

#include "base/types.hpp"
#include "peaks.hpp"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>

namespace pierre {

namespace fft {
enum class direction { Reverse, Forward };
enum class window {
  Rectangle,        // rectangle (Box car)
  Hamming,          // hamming
  Hann,             // hann
  Triangle,         // triangle (Bartlett)
  Nuttall,          // nuttall
  Blackman,         // blackman
  Blackman_Nuttall, // blackman nuttall
  Blackman_Harris,  // blackman harris
  Flat_top,         // flat top
  Welch             // welch
};

} // namespace fft

class FFT {
public:
  FFT(const float *reals, size_t samples, const float frequency);

  void compute(fft::direction dir); // computes in-place complex-to-complex FFT
  peaks_t find_peaks();

  static void init();

  void process();

private:
  void complex_to_magnitude();
  void dc_removal() noexcept;
  Freq freq_at_index(size_t y);
  Mag mag_at_index(const size_t i) const;
  void windowing(fft::direction dir);

private:
  static constexpr float sq(const float x) { return x * x; }

private:
  // order dependent
  const float _sampling_freq;

  const size_t _max_peaks;
  reals_t _imaginary;
  uint_fast8_t _power;

  reals_t _reals;
};

} // namespace pierre
