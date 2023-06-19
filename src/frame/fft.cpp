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

namespace pierre {

reals_t FFT::wwf;

void FFT::init() noexcept {

  if (wwf.size() == 0) {
    constexpr auto sq = [](auto x) { return x * x; };

    wwf.reserve(samples_max >> 1);
    auto wwf_it = std::back_inserter(wwf);

    double samplesMinusOne = samples_max - 1.0;
    double compensationFactor = win_compensation_factors[window_type];

    for (size_t i = 0; i < (samples_max >> 1); i++) {
      double indexMinusOne = i;
      double ratio = (indexMinusOne / samplesMinusOne);
      double weighingFactor = 1.0;
      // Compute and record weighting factor
      switch (window_type) {
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
        weighingFactor =
            0.2810639 - (0.5208972 * cos(PI2 * ratio)) + (0.1980399 * cos(PI4 * ratio));
        break;
      case window::Welch: // welch
        weighingFactor =
            1.0 - (sq(indexMinusOne - samplesMinusOne / 2.0) / (samplesMinusOne / 2.0));
        break;

      default:
        weighingFactor = Hann;
        break;
      }

      wwf_it = with_compensation ? (weighingFactor * compensationFactor) : weighingFactor;
    }
  }
}

} // namespace pierre
