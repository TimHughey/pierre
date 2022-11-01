
// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/color.hpp"
#include "base/elapsed.hpp"
#include "base/types.hpp"
#include "desk/fx.hpp"
#include "frame/peaks.hpp"

#include <boost/circular_buffer.hpp>

namespace pierre {
namespace fx {

class MajorPeak : public FX {
public:
  using circular_buffer = boost::circular_buffer<Peak>;

public:
  struct WhenGreater {
    Frequency frequency;
    double brightness_min;
    struct {
      double brightness_min;
    } higher_frequency;
  };

  struct WhenLessThan {
    Frequency frequency;
    double brightness_min;
  };

  struct FillPinspot {
    string name{"fill pinspot"};
    uint32_t fade_max_ms = 800;
    Frequency frequency_max = 1000.0;

    struct WhenGreater when_greater {
      .frequency = 180.0, .brightness_min = 3.0, .higher_frequency = {.brightness_min = 80.0 }
    };

    struct WhenLessThan when_lessthan {
      .frequency = 180.0, .brightness_min = 27.0
    };
  };

  struct WhenFading {
    double brightness_min;
    struct {
      double brightness_min;
    } frequency_greater;
  };

  struct MainPinspot {
    string name{"main pinspot"};
    uint32_t fade_max_ms = 700;
    Frequency frequency_min = 180.0;

    struct WhenFading when_fading {
      .brightness_min = 5.0, .frequency_greater = {.brightness_min = 69.0 }
    };

    struct WhenLessThan when_lessthan {
      .frequency = 180.0, .brightness_min = 27.0
    };
  };

public:
  MajorPeak(pierre::desk::Stats &stats);
  ~MajorPeak();

  void execute(peaks_t peaks) override;
  csv name() const override { return fx::MAJOR_PEAK; }

  void once() override;

private:
  typedef std::vector<Color> ReferenceColors;

private:
  void handleElWire(peaks_t peaks);
  void handleLedForest(peaks_t peaks);
  void handleFillPinspot(peaks_t peaks);
  void handleMainPinspot(peaks_t peaks);

  const Color makeColor(Color ref, const Peak &freq);
  Color &refColor(size_t index) const;

private:
  Color _color;
  pierre::desk::Stats &stats;

  MainPinspot _main_spot_cfg;
  FillPinspot _fill_spot_cfg;

  Elapsed color_elapsed;
  static ReferenceColors _ref_colors;

  circular_buffer _prev_peaks;
  circular_buffer _main_history;
  circular_buffer _fill_history;

public:
  static constexpr csv module_id{"MAJOR_PEAK"};
};

} // namespace fx
} // namespace pierre
