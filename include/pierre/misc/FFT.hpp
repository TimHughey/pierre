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

typedef struct Peak Peak_t;
typedef float Freq_t;
typedef float dB_t;

struct Peak {
  uint_fast16_t index;
  Freq_t freq;
  dB_t dB;

  static bool higherdB(const Peak_t &lhs, const Peak_t &rhs) {
    return lhs.dB > rhs.dB;
  }

  static const Peak zero() { return Peak{.index = 0, .freq = 0, .dB = 0}; }
};

typedef std::vector<Peak_t> Peaks_t;
typedef std::vector<float> Real_t;
typedef Real_t Imaginary_t;
typedef Real_t WindowWeighingFactors_t;
typedef const Peaks_t PeaksConst_t;
typedef const Peaks_t &PeaksRef_t;
typedef const uint_fast16_t PeakN_t; // represents peak of interest 1..max_peaks
typedef const Peak PeakInfo;
typedef const Peak BinInfo;

// template <typename T> class FFT {
class FFT {
public:
  // Constructor
  FFT(uint_fast16_t samples, float samplingFrequency)
      : _samples(samples), _samplingFrequency(samplingFrequency) {
    // Calculates the base 2 logarithm of sample count
    _power = 0;
    while (((samples >> _power) & 1) != 1) {
      _power++;
    }

    _real.reserve(samples);
    _real.assign(samples, 0);

    _imaginary.reserve(samples);
    _imaginary.assign(samples, 0);

    _wwf.reserve(samples);
    _wwf.assign(samples, 0);

    _peaks.reserve(_peaks_max);
  }

  // Destructor
  ~FFT() {}

  inline BinInfo binInfo(size_t y) {
    BinInfo x = {.index = y, .freq = freqAtIndex(y), .dB = dbAtIndex(y)};

    return std::move(x);
  }

  // Computes in-place complex-to-complex FFT
  void compute(FFTDirection dir);
  void complexToMagnitude();

  inline float dbAtIndex(const size_t i) const {
    const float a = _real[i - 1];
    const float b = _real[i];
    const float c = _real[i + 1];

    const float db = 10 * log10(abs(a - (2.0 * b) + c));
    return db;
  }

  // void dcRemoval() const;
  void dcRemoval(const float mean);
  void findPeaks();

  inline float freqAtIndex(size_t y) {
    const float a = _real[y - 1];
    const float b = _real[y];
    const float c = _real[y + 1];

    float delta = 0.5 * ((a - c) / (a - (2.0 * b) + c));
    float frequency = ((y + delta) * _samplingFrequency) / (_samples - 1);
    if (y == (_samples >> 1)) {
      // To improve calculation on edge values
      frequency = ((y + delta) * _samplingFrequency) / _samples;
    }

    return frequency;
  }

  static inline bool hasPeak(PeaksRef_t p, PeakN_t n) {
    bool rc = false;

    // PeakN_t reprexents the peak of interest in the range of 1..max_peaks
    if (p.size() && (p.size() >= (n - 1))) {
      rc = true;
    }

    return rc;
  }

  static inline bool hasMajorPeak(PeaksRef_t p) { return hasPeak(p, 1); }
  static inline PeakInfo majorPeak(PeaksConst_t &p) { return peakN(p, 1); }

  static inline PeakInfo peakN(PeaksConst_t &p, PeakN_t n) {
    Peak peak = {.index = 0, .freq = 0, .dB = 0};

    if (hasPeak(p, n)) {
      peak = p.at(n - 1);
    }

    return std::move(peak);
  }

  inline const Peaks_t &peaks() const { return _peaks; }

  inline void process() {
    double mean = 0.0;

    for (auto val : _real) {
      mean += val;
    }

    mean /= _samples;

    _imaginary.assign(_samples, 0);
    dcRemoval(mean);
    windowing(FFTWindow::Blackman_Harris, FFTDirection::Forward);
    compute(FFTDirection::Forward);
    complexToMagnitude();
    findPeaks();
  }

  Real_t &real() { return _real; }

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

  // static inline void swap(float &x, float &y) {
  //   const float temp = x;
  //   x = y;
  //   y = temp;
  // }

  inline float sq(const float x) const { return x * x; }

  /* Variables */
  Real_t _real;
  Imaginary_t _imaginary;
  WindowWeighingFactors_t _wwf;
  uint_fast16_t _samples = 0;
  float _samplingFrequency = 0;

  FFTWindow _weighingFactorsFFTWindow;
  bool _weighingFactorsWithCompensation = false;
  bool _weighingFactorsComputed = false;
  uint_fast8_t _power = 0;

  Peaks_t _peaks;

  const size_t _peaks_max = (_samples >> 1) + (_samples >> 2);
};

#endif
