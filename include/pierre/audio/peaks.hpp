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
#include <iostream>
#include <iterator>
#include <vector>

#include "local/types.hpp"
#include "misc/minmax.hpp"

namespace pierre {
namespace audio {
typedef float Freq_t;
typedef float Mag_t;
typedef float MagScaled;
typedef size_t PeakN; // represents peak of interest 1..max_peaks

struct Peak {
  struct Config {
    Config() = default;
    Config(const Config &);

    struct {
      std::shared_ptr<MinMaxFloat> minmax;
      Mag_t strong;
    } mag;

    struct {
      Mag_t factor;
      std::shared_ptr<MinMaxFloat> minmax;
      Mag_t step;
    } scale;

    auto &activeScale() const { return scale.minmax; }

    static Config defaults();

    const Mag_t &ceiling() const { return mag.minmax->max(); }
    const Mag_t &floor() const { return mag.minmax->min(); }

    Config &operator=(const Config &rhs);

    void reset() { *this = defaults(); }

    auto scaleCeiling() const { return scale.minmax->max(); }
    auto scaleFloor() const { return scale.minmax->min(); }
    auto scaleFactor() const { return scale.factor; }

    void scaleIncrease() {
      auto step = scale.step;
      auto &factor = scale.factor;
      factor += step;

      auto new_floor = scaleMagVal(floor() * factor);
      auto new_ceiling = scaleMagVal(ceiling());
      scale.minmax->set(new_floor, new_ceiling);
    }

    void scaleReduce() {
      auto step = scale.step;
      auto &factor = scale.factor;
      factor -= step;

      auto new_floor = scaleMagVal(floor() * factor);
      auto new_ceiling = scaleMagVal(ceiling());
      scale.minmax->set(new_floor, new_ceiling);
    }

    Mag_t step() const { return scale.step; }
    Mag_t strong() const { return mag.strong; }
  };

public: // Peak
  size_t index = 0;
  Freq_t freq = 0;
  Mag_t mag = 0;

  Peak() = default;

  Peak(const size_t i, const Freq_t f, const Mag_t m)
      : index(i), freq(f), mag(m) {}

  static std::shared_ptr<MinMaxFloat> activeScale() {
    return _cfg.activeScale();
  }

  static Config &config() { return _cfg; }

  static Mag_t magFloor() { return _cfg.floor(); }
  MagScaled magScaled() const { return scaleMagVal(mag); }
  bool magStrong() const { return mag >= (_cfg.floor() * _cfg.strong()); }

  explicit operator bool() const {
    auto rc = false;

    if (mag > _cfg.floor()) {
      rc = true;
    }

    return rc;
  }

  static const Peak zero() { return std::move(Peak()); }

private:
  static Mag_t scaleMagVal(Mag_t mag) { return 10.0f * log10(mag); }

  static Config _cfg;
};

class Peaks {
public:
  Peaks();
  ~Peaks() = default;

  void analyzeMagnitudes();

  bool bass() const;
  auto cbegin() const { return _peaks.cbegin(); }
  auto cend() const { return _peaks.cend(); }
  bool hasPeak(PeakN n) const;
  const Peak majorPeak() const;
  const Peak peakN(const PeakN n) const;
  auto size() const { return _peaks.size(); }

  void sort();
  void push_back(const Peak &peak) { _peaks.push_back(peak); }

private:
  std::vector<Peak> _peaks;
  std::vector<uint16_t> _mag_histogram;
};

typedef std::shared_ptr<Peaks> spPeaks;

} // namespace audio
} // namespace pierre

#endif
