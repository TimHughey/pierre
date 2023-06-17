
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

#include "base/conf/token.hpp"
#include "base/elapsed.hpp"
#include "base/types.hpp"
#include "desk/color/hsb.hpp"
#include "desk/fx.hpp"
#include "desk/fx/majorpeak/fdecls.hpp"
#include "frame/peaks.hpp"

#include <array>
#include <atomic>
#include <memory>
#include <optional>

namespace pierre {

namespace desk {

/// @brief Primary rendering FX.
///        MajorPeak uses the FFT peaks to render colors via
///        the pinspots and adjust the EL wire based on the magnitudes.
///
///        The rendering takes into account the previously selected
///        pinspot colors, peaks and magnitude to create a dynamic
///        lightshow from the audio.
///
///        Supports runtime changing of configuration.
class MajorPeak : public FX {

  enum pinspot_t : uint8_t { Fill = 0, Main, EndSpots };

public:
  /// @brief Construct the object
  MajorPeak() noexcept;

  ~MajorPeak() noexcept override;

  const auto &runtime_conf() const noexcept { return *runconf; }

  /// @brief Render one frame
  /// @param peaks Peaks to consider for rendering
  void execute(const Peaks &peaks) noexcept override;

  /// @brief Consume frame 0 to perform initialization
  /// @return always true
  bool once() noexcept override;

private:
  using RefColors = std::vector<Hsb>;

private:
  /// @brief Control intensity of the EL wire based on the peak magnitudes
  /// @param peaks Peaks to consider
  void handle_el_wire(const Peaks &peaks);

  /// @brief Control the color and fading of the "Fill" pinspot.
  ///        The "Fill" and "Main" pinspots use complimentary algorithms
  ///        to select the color and duration of a rendered peak.
  ///
  ///        In general the "Fill" pinspot changes less frequently
  ///        than the "Main" pinspot.
  ///
  ///        A variety of configuration parameters can be adjusted
  ///        to vary the rendering.
  /// @param peaks Peaks to consider
  /// void handle_fill_pinspot(const Peaks &peaks);

  /// @brief Control the color and fading of the "Main" pinspot.
  ///        The "Main" and "Fill" pinspots use complimentary algorithms
  ///        to select the color and duration of a rendered peak.
  ///
  ///        In general the "Main" pinspot is "active", changing more often
  ///        and fading slightly faster than the "Fill" pinspot.
  ///
  ///        A variety of configuration parameters can be adjusted
  ///        to vary the rendering.
  /// @param peaks Peaks to consider
  /// void handle_main_pinspot(const Peaks &peaks);

  /// @brief Load the configuration at start and when on-disk conf changes
  /// @return boolean, true when configuration parsed successfully
  /// bool load_config() noexcept;

  /// @brief Make a color from a Peak based on frequency and magnitude
  /// @param peak Peaks to consider
  /// @return Color object
  /// const Color make_color(const Peak &peak) noexcept { return make_color(peak, base_color); }

  /// @brief make a color from a Peak based on frequency and magnitude with a reference color.
  /// @param peak Peaks to consider
  /// @param ref Reference color
  /// @return Color object
  Hsb make_color(const spot_spec &ss, const Peak &peak) noexcept;

  /// @brief Access a table of "nice" colors.
  ///        NOTE: This function is legacy however it is kept due to the painstaking
  ///        selection of the reference colors.
  /// @param index Simple integer index into table.
  /// @return Color object
  const Hsb &ref_color(size_t index) const noexcept;

private:
  // order dependent
  conf::token *tokc;
  std::unique_ptr<conf::majorpeak> runconf;
  std::array<Peak, EndSpots> last_peak{};

  // order independent
  static RefColors ref_colors;

  // cached config
  // major_peak::freq_limits_t freq_limits;
  // major_peak::hue_cfg_map hue_cfg_map;
  // mag_min_max mag_limits;
  // major_peak::pspot_cfg_map pspot_cfg_map;

public:
  MOD_ID("fx.majorpeak");
};

} // namespace desk
} // namespace pierre
