#include <limits>

#include <local/types.hpp>

#include "audio/fft.hpp"

using std::swap;

Mag_t Peak::_mag_floor = 36500.0f;
Mag_t Peak::_mag_strong = _mag_floor * 3.0f;

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

FFT::FFT(uint_fast16_t samples, float samplingFrequency)
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

BinInfo FFT::binInfo(size_t i) {
  BinInfo x = Peak(i, freqAtIndex(i), magAtIndex(i));

  return std::move(x);
}

void FFT::complexToMagnitude() {
  // vM is half the size of vReal and vImag
  for (uint_fast16_t i = 0; i < _samples; i++) {
    _real[i] = sqrt(sq(_real[i]) + sq(_imaginary[i]));
  }
}

void FFT::compute(FFTDirection dir) {
  // Reverse bits /
  uint_fast16_t j = 0;
  for (uint_fast16_t i = 0; i < (_samples - 1); i++) {
    if (i < j) {
      swap(_real[i], _real[j]);
      if (dir == FFTDirection::Reverse) {
        swap(_imaginary[i], _imaginary[j]);
      }
    }
    uint_fast16_t k = (_samples >> 1);
    while (k <= j) {
      j -= k;
      k >>= 1;
    }
    j += k;
  }

  // Compute the FFT
  float c1 = -1.0;
  float c2 = 0.0;
  uint_fast16_t l2 = 1;
  for (uint_fast8_t l = 0; (l < _power); l++) {
    uint_fast16_t l1 = l2;
    l2 <<= 1;
    float u1 = 1.0;
    float u2 = 0.0;
    for (j = 0; j < l1; j++) {
      for (uint_fast16_t i = j; i < _samples; i += l2) {
        uint_fast16_t i1 = i + l1;
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
    for (uint_fast16_t i = 0; i < _samples; i++) {
      _real[i] /= _samples;
      _imaginary[i] /= _samples;
    }
  }
}

void FFT::dcRemoval(const float mean) {
  for (uint_fast16_t i = 1; i < ((_samples >> 1) + 1); i++) {
    _real[i] -= mean;
  }
}

void FFT::findPeaks(Peaks &peaks) {
  peaks.clear();

  Mag_t mag_min = std::numeric_limits<float>::max();
  Mag_t mag_max = 0;

  for (uint_fast16_t i = 1; i < ((_samples >> 1) + 1); i++) {
    const float a = _real[i - 1];
    const float b = _real[i];
    const float c = _real[i + 1];

    if ((a < b) && (b > c)) {
      // prevent finding peaks beyond samples / 2 since the
      // results of the FFT are symmetrical
      if (peaks.size() == (_peaks_max - 1)) {
        break;
      }

      Freq_t freq = freqAtIndex(i);
      Mag_t mag = magAtIndex(i);

      mag_min = std::min(mag, mag_min);
      mag_max = std::max(mag, mag_max);

      // if (db >= peak_dB_floor) {
      const Peak peak(i, freq, mag);
      peaks.push_back(peak);
      //}
    }
  }

  std::sort(peaks.begin(), peaks.end(),
            [](const Peak &lhs, const Peak &rhs) { return lhs.mag > rhs.mag; });

  // std::array<char, 128> out;
  // out.fill(0x00);
  // auto p = out.data();
  // for (auto item = peaks.begin(); item <= (peaks.cbegin() + 1); item++) {
  //   auto len = snprintf(p, (out.data() + out.size() - p),
  //                       "freq[%10.2f] mag[%11.2f] ", item->freq, item->mag);
  //   p += len;
  // }
  //
  // if (mag_max > 1000000.0f) {
  //   snprintf(p, (out.data() + out.size() - p), "min[%10.2f] max[%10.2f]",
  //            mag_min, mag_max);
  //
  //   std::cerr << out.data() << std::endl;
  // }
}

Mag_t FFT::magAtIndex(const size_t i) const {
  const float a = _real[i - 1];
  const float b = _real[i];
  const float c = _real[i + 1];

  const Mag_t x = abs(a - (2.0f * b) + c);
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

bool FFT::hasPeak(const Peaks &peaks, PeakN n) {
  bool rc = false;

  // PeakN_t reprexents the peak of interest in the range of 1..max_peaks
  if (peaks.size() && (peaks.size() > n)) {
    rc = true;
  }

  return rc;
}

const Peak FFT::majorPeak(const Peaks &peaks) { return peakN(peaks, 1); }

const Peak FFT::peakN(const Peaks &peaks, PeakN n) {
  Peak peak;

  if (hasPeak(peaks, n)) {
    const Peak check = peaks.at(n - 1);

    if (check.mag > Peak::magFloor()) {
      peak = check;
    }
  }

  return std::move(peak);
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
      for (uint_fast16_t i = 0; i < (_samples >> 1); i++) {
        _real[i] *= _wwf[i];
        _real[_samples - (i + 1)] *= _wwf[i];
      }
    } else {
      for (uint_fast16_t i = 0; i < (_samples >> 1); i++) {
        _real[i] /= _wwf[i];
        _real[_samples - (i + 1)] /= _wwf[i];
      }
    }
  } else {
    // no. values need to be pre-computed or applied
    float samplesMinusOne = (float(_samples) - 1.0);
    float compensationFactor =
        _winCompensationFactors[static_cast<uint_fast8_t>(windowType)];
    for (uint_fast16_t i = 0; i < (_samples >> 1); i++) {
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
