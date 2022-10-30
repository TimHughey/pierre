/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
    Copyright (C) 2020  Tim Hughey

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

#include "frame/fft.hpp"
#include "base/logger.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <numbers>
#include <numeric>
#include <ranges>
#include <thread>

namespace pierre {

using namespace fft;

static const float PI2 = std::numbers::pi * 2;
static const float PI4 = std::numbers::pi * 4;
static const float PI6 = std::numbers::pi * 6;

static const size_t _samples{1024};
static const window _window_type{window::Hann};
static bool _with_compensation = false;
static const float _win_compensation_factors[] = {
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

static reals_t _wwf;
static std::atomic_flag _weighing_factors_computed;

FFT::FFT(const float *reals, size_t samples, const float frequency)
    : _sampling_freq(frequency),  //
      _max_peaks(samples >> 1),   //
      _imaginary(samples, 0),     //
      _power(std::log2f(samples)) //
{
  if (samples != _samples) {
    throw std::runtime_error("unsupported number of samples");
  }

  _reals.reserve(samples);
  for (size_t i = 0; i < samples; i++) {
    _reals.emplace_back(reals[i]);
  }
}

void FFT::complex_to_magnitude() {
  // vM is half the size of vReal and vImag
  for (size_t i = 0; i < _samples; i++) {
    _reals[i] = std::hypot(_reals[i], _imaginary[i]);
  }
}

void FFT::compute(direction dir) {
  // Reverse bits /
  size_t j = 0;
  for (size_t i = 0; i < (_samples - 1); i++) {
    if (i < j) {
      std::swap(_reals[i], _reals[j]);
      if (dir == direction::Reverse) {
        std::swap(_imaginary[i], _imaginary[j]);
      }
    }
    size_t k = (_samples >> 1);
    while (k <= j) {
      j -= k;
      k >>= 1;
    }
    j += k;
  }

  // Compute the FFT
  float c1 = -1.0;
  float c2 = 0.0;
  size_t l2 = 1;
  for (uint_fast8_t l = 0; (l < _power); l++) {
    size_t l1 = l2;
    l2 <<= 1;
    float u1 = 1.0;
    float u2 = 0.0;
    for (j = 0; j < l1; j++) {
      for (size_t i = j; i < _samples; i += l2) {
        size_t i1 = i + l1;
        float t1 = u1 * _reals[i1] - u2 * _imaginary[i1];
        float t2 = u1 * _imaginary[i1] + u2 * _reals[i1];
        _reals[i1] = _reals[i] - t1;
        _imaginary[i1] = _imaginary[i] - t2;
        _reals[i] += t1;
        _imaginary[i] += t2;
      }
      float z = ((u1 * c1) - (u2 * c2));
      u2 = ((u1 * c2) + (u2 * c1));
      u1 = z;
    }

    float cTemp = 0.5 * c1;
    c2 = std::sqrt(0.5 - cTemp);
    c1 = std::sqrt(0.5 + cTemp);

    c2 = dir == direction::Forward ? -c2 : c2;
  }
  // Scaling for reverse transform
  if (dir != direction::Forward) {
    for (size_t i = 0; i < _samples; i++) {
      _reals[i] /= _samples;
      _imaginary[i] /= _samples;
    }
  }
}

void FFT::dc_removal() noexcept {
  double sum = std::accumulate(_reals.begin(), _reals.end(), 0.0);
  double mean = sum / _samples;

  for (size_t i = 1; i < ((_samples >> 1) + 1); i++) {
    _reals[i] -= mean;
  }
}

peaks_t FFT::find_peaks() {
  auto peaks = Peaks::create();

  // result of fft is symmetrical, look at first half only
  for (size_t i = 1; i < ((_samples >> 1) + 1); i++) {
    const float a = _reals[i - 1];
    const float b = _reals[i];
    const float c = _reals[i + 1];

    if ((a < b) && (b > c)) {
      peaks->emplace(mag_at_index(i), freq_at_index(i));
    }
  }

  return peaks;
}

void FFT::init() { // static

  _wwf.reserve(_samples);
  auto wwf = std::back_inserter(_wwf);

  float samplesMinusOne = (float(_samples) - 1.0);
  float compensationFactor = _win_compensation_factors[static_cast<uint_fast8_t>(_window_type)];
  for (size_t i = 0; i < (_samples >> 1); i++) {
    float indexMinusOne = float(i);
    float ratio = (indexMinusOne / samplesMinusOne);
    float weighingFactor = 1.0;
    // Compute and record weighting factor
    switch (_window_type) {
    case window::Rectangle: // rectangle (box car)
      weighingFactor = 1.0;
      break;
    case window::Hamming: // hamming
      weighingFactor = 0.54 - (0.46 * cos(PI2 * ratio));
      break;
    case window::Hann: // hann
      weighingFactor = 0.54 * (1.0 - cos(PI2 * ratio));
      break;
    case window::Triangle: // triangle (Bartlett)
      weighingFactor =
          1.0 - ((2.0 * abs(indexMinusOne - (samplesMinusOne / 2.0))) / samplesMinusOne);
      break;
    case window::Nuttall: // nuttall
      weighingFactor = 0.355768 - (0.487396 * (cos(PI2 * ratio))) +
                       (0.144232 * (cos(PI4 * ratio))) - (0.012604 * (cos(PI6 * ratio)));
      break;
    case window::Blackman: // blackman
      weighingFactor = 0.42323 - (0.49755 * (cos(PI2 * ratio))) + (0.07922 * (cos(PI4 * ratio)));
      break;
    case window::Blackman_Nuttall: // blackman nuttall
      weighingFactor = 0.3635819 - (0.4891775 * (cos(PI2 * ratio))) +
                       (0.1365995 * (cos(PI4 * ratio))) - (0.0106411 * (cos(PI6 * ratio)));
      break;
    case window::Blackman_Harris: // blackman harris
      weighingFactor = 0.35875 - (0.48829 * (cos(PI2 * ratio))) + (0.14128 * (cos(PI4 * ratio))) -
                       (0.01168 * (cos(PI6 * ratio)));
      break;
    case window::Flat_top: // flat top
      weighingFactor = 0.2810639 - (0.5208972 * cos(PI2 * ratio)) + (0.1980399 * cos(PI4 * ratio));
      break;
    case window::Welch: // welch
      weighingFactor = 1.0 - (sq(indexMinusOne - samplesMinusOne / 2.0) / (samplesMinusOne / 2.0));
      break;
    }
    if (_with_compensation) {
      weighingFactor *= compensationFactor;
    }

    wwf = weighingFactor;
  }

  INFO("FFT", "INIT", "wwf_size={}\n", _wwf.size());

  // _weighing_factors_computed.test_and_set();
  // _weighing_factors_computed.notify_all();
}

Magnitude FFT::mag_at_index(const size_t i) const {
  const float a = _reals[i - 1];
  const float b = _reals[i];
  const float c = _reals[i + 1];

  return Magnitude(abs(a - (2.0f * b) + c));
}

Frequency FFT::freq_at_index(size_t y) {
  const float a = _reals[y - 1];
  const float b = _reals[y];
  const float c = _reals[y + 1];

  float delta = 0.5 * ((a - c) / (a - (2.0 * b) + c));
  float frequency = ((y + delta) * _sampling_freq) / (_samples - 1);
  if (y == (_samples >> 1)) {
    // To improve calculation on edge values
    frequency = ((y + delta) * _sampling_freq) / _samples;
  }

  return Frequency(frequency);
}

void FFT::process() {
  dc_removal();
  windowing(direction::Forward);
  compute(direction::Forward);
  complex_to_magnitude();
}

void FFT::windowing(direction dir) {
  if (dir == direction::Forward) {
    for (size_t i = 0; i < (_samples >> 1); i++) {
      _reals[i] *= _wwf[i];
      _reals[_samples - (i + 1)] *= _wwf[i];
    }
  } else {
    for (size_t i = 0; i < (_samples >> 1); i++) {
      _reals[i] /= _wwf[i];
      _reals[_samples - (i + 1)] /= _wwf[i];
    }
  }
}

} // namespace pierre
