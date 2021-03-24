/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#ifndef pierre_audio_peaks_hpp
#define pierre_audio_peaks_hpp

#include <algorithm>
#include <cmath>
#include <vector>

#include "local/types.hpp"

namespace pierre {
namespace audio {
typedef float Freq_t;
typedef float Mag_t;
typedef float MagScaled;
typedef size_t PeakN; // represents peak of interest 1..max_peaks

struct Peak {
  struct Config {
    struct {
      Mag_t floor;
      Mag_t strong;
    } mag;
  };

  struct Scale {
    Mag_t min = scale(36500.0f);
    Mag_t max = scale(1500000.0f);
  };

  size_t index = 0;
  Freq_t freq = 0;
  Mag_t mag = 0;

  Peak() = default;

  Peak(const size_t i, const Freq_t f, const Mag_t m)
      : index(i), freq(f), mag(m) {}

  static Mag_t magFloor() { return _cfg.mag.floor; }
  MagScaled magScaled() const { return scale(mag); }
  bool magStrong() const { return mag >= (_cfg.mag.floor * _cfg.mag.strong); }

  explicit operator bool() const {
    auto rc = false;

    if (mag > _cfg.mag.floor) {
      rc = true;
    }

    return rc;
  }

  static Scale scale() { return std::move(Scale()); }
  static const Peak zero() { return std::move(Peak()); }

private:
  static Mag_t scale(Mag_t mag) { return 10.0f * log10(mag); }

  static Config _cfg;
};

class Peaks {
public:
  Peaks() = default;
  ~Peaks() = default;

  bool bass() const;
  bool hasPeak(PeakN n) const;
  const Peak majorPeak() const;
  const Peak peakN(const PeakN n) const;
  size_t size() const { return _peaks.size(); }

  void sort();
  void push_back(const Peak &peak) { _peaks.push_back(peak); }

private:
  std::vector<Peak> _peaks;
};

typedef std::shared_ptr<Peaks> spPeaks;

} // namespace audio
} // namespace pierre

#endif
