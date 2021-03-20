/*

        FFT libray
        Copyright (C) 2010 Didier Longueville
        Copyright (C) 2014 Enrique Condes
        Copyright (C) 2020 Bim Overbohm (header-only, template, speed
   improvements)

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

*/

#ifndef _pierre_fft_h
#define _pierre_fft_h

#include <algorithm>
#include <functional>
#include <vector>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "local/types.hpp"
#include "misc/maverage.hpp"

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

typedef float Freq_t;
typedef float Mag_t;
typedef float MagScaled;

struct Peak {
  struct Scale {
    Mag_t min = scale(36500.0f);
    Mag_t max = scale(1500000.0f);
  };

  uint_fast16_t index = 0;
  Freq_t freq = 0;
  Mag_t mag = 0;

  Peak() = default;

  Peak(const uint_fast16_t i, const Freq_t f, const Mag_t m)
      : index(i), freq(f), mag(m) {}

  static Mag_t magFloor() { return _mag_floor; }
  MagScaled magScaled() const { return scale(mag); }
  bool magStrong() const { return mag >= _mag_strong; }

  explicit operator bool() const {
    auto rc = false;

    if (mag > _mag_floor) {
      rc = true;
    }

    return rc;
  }

  static Scale scale() { return std::move(Scale()); }
  static const Peak zero() { return std::move(Peak()); }

private:
  static Mag_t scale(Mag_t mag) { return 10.0f * log10(mag); }

  static Mag_t _mag_floor;
  static Mag_t _mag_strong;
};

typedef std::vector<Peak> Peaks;
typedef std::vector<float> Real_t;
typedef Real_t Imaginary_t;
typedef Real_t WindowWeighingFactors_t;
typedef const uint_fast16_t PeakN; // represents peak of interest 1..max_peaks
typedef const Peak BinInfo;

// template <typename T> class FFT {
class FFT {
public:
  // Constructor
  FFT(uint_fast16_t samples, float samplingFrequency);

  // Destructor
  ~FFT() {}

  BinInfo binInfo(size_t y);

  // Computes in-place complex-to-complex FFT
  void compute(FFTDirection dir);
  void complexToMagnitude();

  void dcRemoval(const float mean);
  void findPeaks(Peaks &peaks);
  Freq_t freqAtIndex(size_t y);
  bool hasPeak(const Peaks &peaks, PeakN n);
  Mag_t magAtIndex(const size_t i) const;
  const Peak majorPeak(const Peaks &peaks);
  const Peak peakN(const Peaks &peaks, PeakN n);
  void process();
  Real_t &real();

  // Get library revision
  static uint8_t revision() { return 0x27; }

  void windowing(FFTWindow windowType, FFTDirection dir,
                 bool withCompensation = false);

private:
  static const float _winCompensationFactors[10];

  // Mathematial constants
  static constexpr float TWO_PI = 6.28318531;
  static constexpr float FOUR_PI = 12.56637061;
  static constexpr float SIX_PI = 18.84955593;

  inline float sq(const float x) const { return x * x; }

  /* Variables */
  Real_t _real;
  Imaginary_t _imaginary;
  static WindowWeighingFactors_t _wwf;
  uint_fast16_t _samples;
  float _samplingFrequency;

  FFTWindow _weighingFactorsFFTWindow;
  bool _weighingFactorsWithCompensation = false;
  static bool _weighingFactorsComputed;
  uint_fast8_t _power = 0;
  const size_t _max_num_peaks = _samples >> 1;
};

#endif
