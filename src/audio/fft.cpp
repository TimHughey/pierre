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

#include <limits>

#include "audio/fft.hpp"

namespace pierre {
namespace audio {

using std::swap;

const float FFT::_winCompensationFactors[10] = {
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

WindowWeighingFactors_t FFT::_wwf;
bool FFT::_weighingFactorsComputed = false;

FFT::FFT(size_t samples, float samplingFrequency)
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

  if (_wwf.size() != samples) {
    _wwf.reserve(samples);
    _wwf.assign(samples, 0);
  }
}

void FFT::complexToMagnitude() {
  // vM is half the size of vReal and vImag
  for (size_t i = 0; i < _samples; i++) {
    _real[i] = sqrt(sq(_real[i]) + sq(_imaginary[i]));
  }
}

void FFT::compute(FFTDirection dir) {
  // Reverse bits /
  size_t j = 0;
  for (size_t i = 0; i < (_samples - 1); i++) {
    if (i < j) {
      swap(_real[i], _real[j]);
      if (dir == FFTDirection::Reverse) {
        swap(_imaginary[i], _imaginary[j]);
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
        float t1 = u1 * _real[i1] - u2 * _imaginary[i1];
        float t2 = u1 * _imaginary[i1] + u2 * _real[i1];
        _real[i1] = _real[i] - t1;
        _imaginary[i1] = _imaginary[i] - t2;
        _real[i] += t1;
        _imaginary[i] += t2;
      }
      float z = ((u1 * c1) - (u2 * c2));
      u2 = ((u1 * c2) + (u2 * c1));
      u1 = z;
    }

    float cTemp = 0.5 * c1;
    c2 = sqrt(0.5 - cTemp);
    c1 = sqrt(0.5 + cTemp);

    c2 = dir == FFTDirection::Forward ? -c2 : c2;
  }
  // Scaling for reverse transform
  if (dir != FFTDirection::Forward) {
    for (size_t i = 0; i < _samples; i++) {
      _real[i] /= _samples;
      _imaginary[i] /= _samples;
    }
  }
}

void FFT::dcRemoval(const float mean) {
  for (size_t i = 1; i < ((_samples >> 1) + 1); i++) {
    _real[i] -= mean;
  }
}

void FFT::findPeaks(spPeaks peaks) {
  Mag mag_min = std::numeric_limits<float>::max();
  Mag mag_max = 0;

  for (size_t i = 1; i < ((_samples >> 1) + 1); i++) {
    const float a = _real[i - 1];
    const float b = _real[i];
    const float c = _real[i + 1];

    if ((a < b) && (b > c)) {
      // prevent finding peaks beyond samples / 2 since the
      // results of the FFT are symmetrical
      if (peaks->size() == (_max_num_peaks - 1)) {
        break;
      }

      Freq freq = freqAtIndex(i);
      Mag mag = magAtIndex(i);

      mag_min = std::min(mag, mag_min);
      mag_max = std::max(mag, mag_max);

      const Peak peak(i, freq, mag);
      peaks->push_back(peak);
    }
  }

  peaks->sort();
}

Mag FFT::magAtIndex(const size_t i) const {
  const float a = _real[i - 1];
  const float b = _real[i];
  const float c = _real[i + 1];

  const Mag x = abs(a - (2.0f * b) + c);
  return x;
}

float FFT::freqAtIndex(size_t y) {
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

void FFT::process() {
  double mean = 0.0;

  for (auto val : _real) {
    mean += val;
  }

  mean /= _samples;

  _imaginary.assign(_samples, 0x00);
  dcRemoval(mean);
  windowing(FFTWindow::Blackman, FFTDirection::Forward);
  compute(FFTDirection::Forward);
  complexToMagnitude();
}

Real_t &FFT::real() { return _real; }

void FFT::windowing(FFTWindow windowType, FFTDirection dir,
                    bool withCompensation) {
  // check if values are already pre-computed for the correct window type and
  // compensation
  if (_weighingFactorsComputed && _weighingFactorsFFTWindow == windowType &&
      _weighingFactorsWithCompensation == withCompensation) {
    // yes. values are precomputed
    if (dir == FFTDirection::Forward) {
      for (size_t i = 0; i < (_samples >> 1); i++) {
        _real[i] *= _wwf[i];
        _real[_samples - (i + 1)] *= _wwf[i];
      }
    } else {
      for (size_t i = 0; i < (_samples >> 1); i++) {
        _real[i] /= _wwf[i];
        _real[_samples - (i + 1)] /= _wwf[i];
      }
    }
  } else {
    // no. values need to be pre-computed or applied
    float samplesMinusOne = (float(_samples) - 1.0);
    float compensationFactor =
        _winCompensationFactors[static_cast<uint_fast8_t>(windowType)];
    for (size_t i = 0; i < (_samples >> 1); i++) {
      float indexMinusOne = float(i);
      float ratio = (indexMinusOne / samplesMinusOne);
      float weighingFactor = 1.0;
      // Compute and record weighting factor
      switch (windowType) {
      case FFTWindow::Rectangle: // rectangle (box car)
        weighingFactor = 1.0;
        break;
      case FFTWindow::Hamming: // hamming
        weighingFactor = 0.54 - (0.46 * cos(TWO_PI * ratio));
        break;
      case FFTWindow::Hann: // hann
        weighingFactor = 0.54 * (1.0 - cos(TWO_PI * ratio));
        break;
      case FFTWindow::Triangle: // triangle (Bartlett)
        weighingFactor =
            1.0 - ((2.0 * abs(indexMinusOne - (samplesMinusOne / 2.0))) /
                   samplesMinusOne);
        break;
      case FFTWindow::Nuttall: // nuttall
        weighingFactor = 0.355768 - (0.487396 * (cos(TWO_PI * ratio))) +
                         (0.144232 * (cos(FOUR_PI * ratio))) -
                         (0.012604 * (cos(SIX_PI * ratio)));
        break;
      case FFTWindow::Blackman: // blackman
        weighingFactor = 0.42323 - (0.49755 * (cos(TWO_PI * ratio))) +
                         (0.07922 * (cos(FOUR_PI * ratio)));
        break;
      case FFTWindow::Blackman_Nuttall: // blackman nuttall
        weighingFactor = 0.3635819 - (0.4891775 * (cos(TWO_PI * ratio))) +
                         (0.1365995 * (cos(FOUR_PI * ratio))) -
                         (0.0106411 * (cos(SIX_PI * ratio)));
        break;
      case FFTWindow::Blackman_Harris: // blackman harris
        weighingFactor = 0.35875 - (0.48829 * (cos(TWO_PI * ratio))) +
                         (0.14128 * (cos(FOUR_PI * ratio))) -
                         (0.01168 * (cos(SIX_PI * ratio)));
        break;
      case FFTWindow::Flat_top: // flat top
        weighingFactor = 0.2810639 - (0.5208972 * cos(TWO_PI * ratio)) +
                         (0.1980399 * cos(FOUR_PI * ratio));
        break;
      case FFTWindow::Welch: // welch
        weighingFactor = 1.0 - (sq(indexMinusOne - samplesMinusOne / 2.0) /
                                (samplesMinusOne / 2.0));
        break;
      }
      if (withCompensation) {
        weighingFactor *= compensationFactor;
      }

      _wwf[i] = weighingFactor;

      if (dir == FFTDirection::Forward) {
        _real[i] *= weighingFactor;
        _real[_samples - (i + 1)] *= weighingFactor;
      } else {
        _real[i] /= weighingFactor;
        _real[_samples - (i + 1)] /= weighingFactor;
      }
    }
    // mark cached values as pre-computed
    _weighingFactorsFFTWindow = windowType;
    _weighingFactorsWithCompensation = withCompensation;
    _weighingFactorsComputed = true;
  }
}

} // namespace audio
} // namespace pierre
