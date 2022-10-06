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

#include "base/typical.hpp"
#include "peaks.hpp"

#include <algorithm>
#include <functional>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>

namespace pierre {

enum class FFTDirection { Reverse, Forward };

enum class FFTWindow {
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

typedef Reals Imaginary_t;
typedef Reals WindowWeighingFactors_t;

class FFT {
public:
  FFT(size_t samples, float samplingFrequency);
  ~FFT() = default;

  static void init();

  // Computes in-place complex-to-complex FFT
  void compute(FFTDirection dir);
  void complexToMagnitude();

  void dcRemoval(const float mean);

  peaks_t find_peaks();
  Freq freqAtIndex(size_t y);
  Mag magAtIndex(const size_t i) const;
  void process();
  Reals &real() { return _real; }

  void windowing(FFTWindow windowType, FFTDirection dir, bool withCompensation = false);

private:
  static const float _winCompensationFactors[10];

  inline float sq(const float x) const { return x * x; }

  /* Variables */
  Reals _real;
  Imaginary_t _imaginary;
  static WindowWeighingFactors_t _wwf;
  size_t _samples;
  float _samplingFrequency;

  FFTWindow _weighingFactorsFFTWindow;
  bool _weighingFactorsWithCompensation = false;
  static bool _weighingFactorsComputed;
  uint_fast8_t _power = 0;
  const size_t _max_num_peaks = _samples >> 1;
};

} // namespace pierre
