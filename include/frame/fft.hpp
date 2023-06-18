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

#include "base/input_info.hpp"
#include "base/types.hpp"
#include "peaks.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <math.h>
#include <numbers>
#include <numeric>
#include <tuple>
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
    Welch,            // welch
    UnknownWindow     // Uknown
  };

  static constexpr std::array<const string, UnknownWindow + 1> win_types{
      "Rectangle",       "Hamming",         "Hann",     "Triangle", "Nutall",        "Blackman",
      "Blackman_Nutall", "Blackman_Harris", "Flat_top", "Welch",    "Unknown_Window"};

public:
  /// @brief Create FFT processor and initialize base data, as needed
  /// @param reals_in Raw pointer to arary of float
  /// @param samples_in Count of samples to process
  /// @param sampling_freq_in Sample frequency (e.g. 44100 Hz)
  template <typename T>
  FFT(const float *reals_in, size_t samples_in, float sampling_freq_in, const T &win) noexcept
      : // allocate reals, we write directly into container
        reals(samples_in, 0),
        // capture sampling frequency
        sampling_freq(sampling_freq_in),
        // determine max supported peaks
        peaks_max(samples_in >> 1),
        // allocate imaginary, we write directly into container
        imaginary(samples_in, 0),
        // calculate the power (number of loops of calculate)
        power(std::log2f(samples_in)),
        // set the windowing type
        window_type{win.wt},
        // apply compenstation factor?
        with_compensation{win.comp} {

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
      const auto a = reals[i - 1];
      const auto b = reals[i];
      const auto c = reals[i + 1];

      if ((a < b) && (b > c)) { // this is a peak

        // frequenxy calculation
        auto delta = 0.5 * ((a - c) / (a - (2.0 * b) + c));
        // improve freq calculation on edge values by adjusting samples_max
        auto samp_max = (i == (samples_max >> 1)) ? samples_max : samples_max - 1;
        auto freq = ((i + delta) * sampling_freq) / samp_max;

        // magnitude and dB calculation
        auto mag = std::abs(a - (2.0 * b) + c);

        // https://www.eevblog.com/forum/beginners/how-to-interpret-the-magnitude-of-fft/
        auto power = std::pow(2.0, InputInfo::bit_depth) / 2.0;
        auto dB_n = 20.0 * (std::log10(mag) - std::log10(samples_max * power));

        // https://www.quora.com/What-is-the-maximum-allowed-audio-amplitude-on-the-standard-audio-CD
        auto dB_abs = dB_n + 96.0;

        // lastly, save the normalized dB, frequency in Peaks
        peaks.insert(peak::Freq(freq), peak::dB(dB_abs), channel);
      }
    }

    peaks.finalize();
  }

  /// @brief Convert a window name to window enum
  /// @param name const string window name
  /// @return window
  static window window_lookup(const string &name) noexcept {
    for (uint_fast8_t i = 0; const auto &win_type : win_types) {
      if (win_type == name) return static_cast<window>(i);
      i++;
    }

    return Hann;
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

  /// @brief Initialize class static data (called once)
  void init() noexcept;

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
