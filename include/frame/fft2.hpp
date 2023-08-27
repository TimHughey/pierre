//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/input_info.hpp"
#include "base/types.hpp"
#include "peaks.hpp"

#include <algorithm>
#include <array>
#include <complex>
#include <fftw3.h>
#include <iterator>
#include <math.h>
#include <numbers>
#include <numeric>
#include <ranges>
#include <tuple>
#include <vector>

namespace pierre {

namespace fft {} // namespace fft

template <typename T>
concept IsFFTWindowConfig = requires(T v) {
  { v.wt };
  { v.comp };
};

class FFT2 {
  static constexpr size_t samples_max{InputInfo::spf};
  using complex = std::complex<double>;

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
    Welch
  };

  static constexpr std::array win_types{
      "Rectangle",        "Hamming",         "Hann",     "Triangle", "Nutall", "Blackman",
      "Blackman_Nuttall", "Blackman_Harris", "Flat_top", "Welch"};

public:
  /// @brief Create FFT processor and initialize base data, as needed
  /// @param reals_in Raw pointer to arary of float
  /// @param samples_in Count of samples to process
  /// @param sampling_freq_in Sample frequency (e.g. 44100 Hz)
  template <typename T>
    requires IsFFTWindowConfig<T>
  FFT2(const float *reals_in, const T &win_conf) noexcept
      : // allocate input buffer with proper alignment
        in{static_cast<double *>(fftw_malloc(sizeof(double) * samples_max))},
        // allocate output buffer (complex numbers) with proper alignment
        out{fftw_alloc_complex(samples_max + 1)},
        // capture sampling frequency
        // sampling_freq(InputInfo::sample_rate),
        // determine max supported peaks
        // peaks_max(samples_max >> 1),
        // allocate imaginary, we write directly into container
        // imaginary(samples_max, 0),
        // calculate the power (number of loops of calculate)
        // power(std::log2f(samples_max)),
        // set the windowing type
        window_type{win_conf.wt},
        // apply compenstation factor?
        with_compensation{win_conf.comp} {

    init(); // calc the window weighing factors, if needed

    // copy the raw source audio to our own buffer for processing
    for (size_t i = 0; i < samples_max; i++) {
      in[i] = reals_in[i];
    }

    fftw_execute_dft_r2c(plan, in, out);
  }

  ~FFT2() noexcept {
    if (in != nullptr) fftw_free(in);
    if (out != nullptr) fftw_free(out);
  }

  /// @brief Find peaks in audio data and populate peaks
  /// @param peaks Reference to Peaks
  /// @param channel Find peaks for channel (Left or Right)
  void find_peaks(Peaks &peaks, Peaks::chan_t channel) noexcept {

    // complex to magnitude, reuse the in array
    for (size_t i = 1; i < (samples_max >> 1) + 1; i++) {
      in[i] = std::abs(*reinterpret_cast<complex *>(&out[i]));
      // reals[i] = std::hypot(reals[i], imaginary[i]);
    }

    // result of fft is symmetrical, look at first half only
    for (size_t i = 1; i < ((samples_max >> 1) + 1); i++) {
      const auto a = in[i - 1];
      const auto b = in[i];
      const auto c = in[i + 1];

      if ((a < b) && (b > c)) { // this is a peak

        // frequenxy calculation
        auto delta = 0.5 * ((a - c) / (a - (2.0 * b) + c));
        // improve freq calculation on edge values by adjusting samples_max
        auto samp_max = (i == (samples_max >> 1)) ? samples_max : samples_max - 1;
        auto freq = ((i + delta) * InputInfo::sample_rate) / samp_max;

        // magnitude calculation
        auto mag = std::abs(a - (2.0 * b) + c);

        // https://www.eevblog.com/forum/beginners/how-to-interpret-the-magnitude-of-fft/
        // auto power = std::pow(2.0, InputInfo::bit_depth) / 2.0;
        // auto dB_n = 20.0 * (std::log10(mag) - std::log10(samples_max * power));

        //
        //  https://www.quora.com/What-is-the-maximum-allowed-audio-amplitude-on-the-standard-audio-CD
        // auto dB_abs = dB_n + 96.0;

        peaks(peak::Freq(freq), peak::Mag(mag), channel);
      }
    }

    peaks.finalize();
  }

  /// @brief Convert a window name to window enum
  /// @param name const string window name
  /// @return window
  static window window_lookup(const string &name) noexcept {
    if (auto it = std::ranges::find(win_types, name); it != win_types.end()) {
      return static_cast<window>(std::distance(win_types.begin(), it));
    }

    return Hann;
  }

private:
  /// @brief Computes in-place complex-to-complex FFT
  /// @param dir Direction (Forward, Reverse)
  /*void compute(direction dir) noexcept {
    // pre-compute transformations
    dc_removal();
    windowing(dir);

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
  }*/

  /// @brief Initialize class static data (called once)
  void init() noexcept;

  /// @brief Apply windowing algorithm
  /// @param dir Direction (Forward or Reverse)
  /*void windowing(direction dir) noexcept {
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
  }*/

private:
  // order dependent
  static fftw_plan plan;
  double *in;
  fftw_complex *out;

  // double sampling_freq;
  // size_t peaks_max;
  // reals_t imaginary;
  // uint_fast8_t power;
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
