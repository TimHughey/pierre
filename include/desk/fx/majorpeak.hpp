
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

#include "base/elapsed.hpp"
#include "io/io.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "desk/color.hpp"
#include "desk/fx.hpp"
#include "lcs/types.hpp"
#include "majorpeak/types.hpp"

#include <atomic>
#include <memory>
#include <optional>

namespace pierre {
namespace fx {

class MajorPeak : public FX, public std::enable_shared_from_this<MajorPeak> {
public:
  MajorPeak(io_context &io_ctx) noexcept;

  void cancel() noexcept override {
    [[maybe_unused]] error_code ec;
    silence_timer.cancel(ec);
  }

  void execute(Peaks &peaks) noexcept override;
  csv name() const override { return fx_name::MAJOR_PEAK; }

  void once() override; // must be in .cpp to limit units include

  std::shared_ptr<FX> ptr_base() noexcept override { return std::static_pointer_cast<FX>(ptr()); }

private:
  using ReferenceColors = std::vector<Color>;

private:
  void handle_el_wire(Peaks &peaks);
  void handle_led_forest(Peaks &peaks);
  void handle_fill_pinspot(Peaks &peaks);
  void handle_main_pinspot(Peaks &peaks);

  void load_config() noexcept;

  const Color make_color(const Peak &peak) noexcept { return make_color(peak, base_color); }
  const Color make_color(const Peak &peak, const Color &ref) noexcept;

  /// @brief Get a shared pointer to this object
  /// @return A shared pointer to this object
  std::shared_ptr<MajorPeak> ptr() noexcept { return shared_from_this(); }

  const Color &ref_color(size_t index) const noexcept;

  void silence_watch() noexcept;

private:
  // order dependent
  io_context &io_ctx;
  steady_timer silence_timer;
  std::atomic_bool silence;
  const Color base_color;
  Peak _main_last_peak;
  Peak _fill_last_peak;

  // order independent
  Elapsed color_elapsed;
  static ReferenceColors _ref_colors;

  // cached config
  cfg_future _cfg_changed;
  major_peak::freq_limits_t _freq_limits;
  major_peak::hue_cfg_map _hue_cfg_map;
  mag_min_max _mag_limits;
  major_peak::pspot_cfg_map _pspot_cfg_map;
  Nanos _silence_timeout;

public:
  static constexpr csv module_id{"major_peak"};
};

} // namespace fx
} // namespace pierre
