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

#include <array>
#include <cassert>
#include <math.h>
#include <numbers>
#include <numeric>
#include <vector>

namespace pierre {

namespace fft {} // namespace fft

class FFT {
  static constexpr size_t samples_max{1024};
  static constexpr auto PI2{std::numbers::pi * 2};
  static constexpr auto PI4{std::numbers::pi * 4};
  static constexpr auto PI6{std::numbers::pi * 6};

public:
  enum direction : uint_fast8_t { Reverse = 0, Forward };
  enum window : uint_fast8_t {
    Rectangle = 0,    // rectangle (Box car)
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

public:
  /// @brief Create FFT processor and initialize base data, as needed
  /// @param reals_in Raw pointer to arary of float
  /// @param samples_in Count of samples to process
  /// @param sampling_freq_in Sample frequency (e.g. 44100 Hz)
  FFT(const float *reals_in, size_t samples_in, const float sampling_freq_in) noexcept
      : // allocate reals, we write directly into container
        reals(samples_max, 0),
        // capture sampling frequency
        sampling_freq(sampling_freq_in),
        // determine max supported peaks
        peaks_max(samples_in >> 1),
        // allocate imaginary, we write directly into container
        imaginary(samples_in, 0),
        // calculate the power (number of loops of calculate)
        power(std::log2f(samples_in)),
        // set the windowing type
        window_type{Blackman_Nuttall},
        // apply compenstation factor?
        with_compensation{true} {

    // abort on samples mismatch
    if (samples_max != samples_in) assert("unsupported number of samples");

    init(); // calc the window weighing factors, if needed

    // copy the raw source audio to our own buffer for processing
    for (size_t i = 0; i < samples_max; i++) {
      reals[i] = reals_in[i];
    }
  }

  /// @brief Find peaks in audio data and populate peaks
  /// @param peaks Reference to Peaks
  /// @param channel Find peaks for channel (Left or Right)
  void find_peaks(Peaks &peaks, Peaks::chan_t channel) noexcept {
    compute(Forward);

    // result of fft is symmetrical, look at first half only
    for (size_t i = 1; i < ((samples_max >> 1) + 1); i++) {
      const double &a = reals[i - 1];
      const double &b = reals[i];
      const double &c = reals[i + 1];

      // this is a peak, insert into Peaks
      if ((a < b) && (b > c)) peaks.insert(mag_at_index(i), freq_at_index(i), channel);
    }
  }

private:
  /// @brief Computes in-place complex-to-complex FFT
  /// @param dir Direction (Forward, Reverse)
  void compute(direction dir) noexcept {
    // pre-compute transformations
    dc_removal();
    windowing(dir);

    // Reverse bits /
    size_t j = 0;
    for (size_t i = 0; i < (samples_max - 1); i++) {
      if (i < j) {
        std::swap(reals[i], reals[j]);
        if (dir == direction::Reverse) std::swap(imaginary[i], imaginary[j]);
      }
      size_t k = (samples_max >> 1);
      while (k <= j) {
        j -= k;
        k >>= 1;
      }
      j += k;
    }

    // Compute the FFT
    double c1 = -1.0;
    double c2 = 0.0;
    size_t l2 = 1;
    for (uint_fast8_t l = 0; (l < power); l++) {
      size_t l1 = l2;
      l2 <<= 1;
      double u1 = 1.0;
      double u2 = 0.0;
      for (j = 0; j < l1; j++) {
        for (size_t i = j; i < samples_max; i += l2) {
          size_t i1 = i + l1;
          double t1 = u1 * reals[i1] - u2 * imaginary[i1];
          double t2 = u1 * imaginary[i1] + u2 * reals[i1];
          reals[i1] = reals[i] - t1;
          imaginary[i1] = imaginary[i] - t2;
          reals[i] += t1;
          imaginary[i] += t2;
        }
        double z = ((u1 * c1) - (u2 * c2));
        u2 = ((u1 * c2) + (u2 * c1));
        u1 = z;
      }

      double cTemp = 0.5 * c1;
      c2 = std::sqrt(0.5 - cTemp);
      c1 = std::sqrt(0.5 + cTemp);

      c2 = dir == direction::Forward ? -c2 : c2;
    }
    // Scaling for reverse transform
    if (dir == direction::Reverse) {
      for (size_t i = 0; i < samples_max; i++) {
        reals[i] /= samples_max;
        imaginary[i] /= samples_max;
      }
    }

    // complex to magnitude
    // vM is half the size of vReal and vImag
    for (size_t i = 0; i < samples_max; i++) {
      reals[i] = std::hypot(reals[i], imaginary[i]);
    }
  }

  /// @brief Remove DC bias in raw audio data
  void dc_removal() noexcept {
    const auto sum = std::accumulate(reals.begin(), reals.end(), 0.0);
    const auto mean = sum / samples_max;

    for (size_t i = 1; i < ((samples_max >> 1) + 1); i++) {
      reals[i] -= mean;
    }
  }

  /// @brief Calculate frequency at index
  /// @param y index, must be > 0
  /// @return Frequency
  Frequency freq_at_index(size_t y) noexcept {
    const double &a = reals[y - 1];
    const double &b = reals[y];
    const double &c = reals[y + 1];

    auto delta = 0.5 * ((a - c) / (a - (2.0 * b) + c));
    auto freq = ((y + delta) * sampling_freq) / (samples_max - 1);

    // to improve calculation on edge values
    return (y == (samples_max >> 1)) ? ((y + delta) * sampling_freq) / samples_max : freq;
  }

  /// @brief Initialize class static data (called once)
  void init() noexcept;

  /// @brief Calculate magnitude at index
  /// @param i index, must be > 0
  /// @return Magnitude
  Magnitude mag_at_index(const size_t i) const noexcept {
    return abs(reals[i - 1] - (2.0 * reals[i]) + reals[i + 1]);
  }

  /// @brief Apply windowing algorithm
  /// @param dir Direction (Forward or Reverse)
  void windowing(direction dir) noexcept {
    if (dir == direction::Forward) {
      for (size_t i = 0; i < (samples_max >> 1); i++) {
        reals[i] *= wwf[i];
        reals[samples_max - (i + 1)] *= wwf[i];
      }
    } else {
      for (size_t i = 0; i < (samples_max >> 1); i++) {
        reals[i] /= wwf[i];
        reals[samples_max - (i + 1)] /= wwf[i];
      }
    }
  }

private:
  // order dependent
  reals_t reals;
  const double sampling_freq;
  const size_t peaks_max;
  reals_t imaginary;
  uint_fast8_t power;
  window window_type;
  bool with_compensation;

  // order independent
  static constexpr std::array win_compensation_factors{
      1.0000000000 * 2.0, // rectangle (Box car)
      1.8549343278 * 2.0, // hamming
      1.8554726898 * 2.0, // hann
      2.0039186079 * 2.0, // triangle (Bartlett)
      2.8163172034 * 2.0, // nuttall
      2.3673474360 * 2.0, // blackman
      2.7557840395 * 2.0, // blackman nuttall
      2.7929062517 * 2.0, // blackman harris
      3.5659039231 * 2.0, // flat top
      1.5029392863 * 2.0  // welch
  };

  /// @brief Window Weighting Factors (calculated by init())
  static reals_t wwf;

public:
  MOD_ID("frame.fft");
};

} // namespace pierre
