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

#pragma once

#include "core/typedefs.hpp"
#include "misc/minmax.hpp"

#include <memory>
#include <type_traits>
#include <vector>

namespace pierre {
namespace player {

typedef float Freq;
typedef float Mag;
typedef float MagScaled;
typedef float Scaled;
typedef float Unscaled;
typedef size_t PeakN; // represents peak of interest 1..max_peaks

namespace peak {
Scaled scaleVal(Unscaled val);

struct MagDetails {
  MinMaxFloat minmax;
  Mag strong;
};

struct ScaleDetails {
  Mag factor;
  MinMaxFloat minmax;
  Mag step;
};

class Config {
public:
  Config() = default;
  Config(const Config &);

  MagDetails mag;
  ScaleDetails scale;

  const auto &activeScale() const { return scale.minmax; }
  const Mag &ceiling() const { return mag.minmax.max(); }
  static Config defaults();
  const Mag &floor() const { return mag.minmax.min(); }

  Config &operator=(const Config &rhs);

  void reset() { *this = defaults(); }

  auto scaleCeiling() const { return scale.minmax.max(); }
  auto scaleFloor() const { return scale.minmax.min(); }
  auto scaleFactor() const { return scale.factor; }

  void scaleIncrease() {
    auto step = scale.step;
    auto &factor = scale.factor;
    factor += step;

    auto new_floor = scaleVal(floor() * factor);
    auto new_ceiling = scaleVal(ceiling());
    scale.minmax.set(new_floor, new_ceiling);
  }

  void scaleReduce() {
    auto step = scale.step;
    auto &factor = scale.factor;
    factor -= step;

    auto new_floor = scaleVal(floor() * factor);
    auto new_ceiling = scaleVal(ceiling());
    scale.minmax.set(new_floor, new_ceiling);
  }

  Mag step() const { return scale.step; }
  Mag strong() const { return mag.strong; }
};
} // namespace peak

struct Peak {
public: // Peak
  Peak() = default;
  Peak(const size_t i, const Freq f, const Mag m);

  static const MinMaxFloat magScaleRange();
  static const MinMaxFloat &activeScale() { return _cfg.activeScale(); }

  static peak::Config &config() { return _cfg; }
  Freq frequency() const { return _freq; }
  Freq frequencyScaled() const { return peak::scaleVal(frequency()); }

  Mag magnitude() const { return _mag; }
  static Mag magFloor() { return _cfg.floor(); }
  MagScaled magScaled() const;
  bool magStrong() const { return _mag >= (_cfg.floor() * _cfg.strong()); }

  explicit operator bool() const;

  template <typename T> const T scaleMagToRange(const MinMaxPair<T> &range) const {
    auto mag_max = _cfg.scaleCeiling();
    auto mag_min = _cfg.scaleFloor();

    auto mag_scaled = peak::scaleVal(_mag);

    auto x =
        ((mag_scaled - mag_min) / (mag_max - mag_min)) * (range.max() - range.min()) + range.min();

    auto ret_val = static_cast<T>(x);

    if (ret_val >= range.max()) {
      ret_val = range.max();
    } else if (ret_val <= range.min()) {
      ret_val = range.min();
    }

    return ret_val;
  }

  static const Peak zero() { return std::move(Peak()); }

private:
  size_t _index = 0;
  Freq _freq = 0;
  Mag _mag = 0;

  static peak::Config _cfg;
};

class Peaks;
typedef std::shared_ptr<Peaks> shPeaks;

class Peaks : public std::enable_shared_from_this<Peaks> {
public:
  static shPeaks create();
  ~Peaks() = default;

private:
  Peaks() = default;

public:
  // Peaks(Peaks &&other) noexcept;
  // Peaks &operator=(Peaks &&rhs) noexcept;

  bool bass() const;
  auto cbegin() const { return _peaks.cbegin(); }
  auto cend() const { return _peaks.cend(); }
  bool hasPeak(PeakN n) const;
  const Peak majorPeak() const;

  // find the first peak greater than the floating point value
  // specified in operator[]
  template <class T, class = typename std::enable_if<std::is_floating_point<T>::value>::type>
  const Peak operator[](T freq) {
    auto found = Peak::zero();

    if (_peaks.size() > 0) {
      auto max_search = _peaks.cbegin() + 5;
      for (auto s = _peaks.begin(); s <= max_search; s++) {
        // stop searching once we've reached peaks with magnitudes less than
        // the configured magFloor()
        if (s->magnitude() < Peak::magFloor()) {
          break;
        }

        if (s->frequency() > freq) {
          found = *s;
          break;
        }
      }
    }

    // ensure the peak found conforms to the magnitude ranges
    if (found) {
      return found;
    } else {
      return Peak::zero();
    }
  }

  const Peak peakN(const PeakN n) const;
  void push_back(const Peak &peak) { _peaks.push_back(peak); }
  static bool silence(shPeaks peaks) {
    auto rc = true;

    if (peaks.use_count()) {
      rc = !peaks->hasPeak(1);
    }

    __LOGX("{:<18} use_count={} hasPeak={}\n", moduleId, //
           peaks.use_count(), peaks.use_count() ? peaks->hasPeak(1) : "NA");

    return rc;
  }

  auto size() const { return _peaks.size(); }

  std::shared_ptr<Peaks> sort();

private:
  std::vector<Peak> _peaks;

  static constexpr csv moduleId = csv("PEAKS");
};

} // namespace player
} // namespace pierre
