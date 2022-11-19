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

#include "desk/fx/majorpeak.hpp"
#include "base/elapsed.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "desk/desk.hpp"
#include "desk/unit/all.hpp"
#include "fader/easings.hpp"
#include "fader/toblack.hpp"
#include "fx/majorpeak/config.hpp"
#include "stats/stats.hpp"

namespace pierre {

using FillFader = fader::ToBlack<fader::SimpleLinear>;
using MainFader = fader::ToBlack<fader::SimpleLinear>;

namespace fx {

namespace {
namespace stats = pierre::stats;
}

MajorPeak::MajorPeak() noexcept
    : FX(),                         //
      base_color(Hsb{0, 100, 100}), //
      _main_last_peak(),            //
      _fill_last_peak()             //
{
  // initialize static frequency to color mapping
  if (_ref_colors.size() == 0) {
    _ref_colors.assign( //
        {Color(0xff0000), Color(0xdc0a1e), Color(0xff002a), Color(0xb22222), Color(0xdc0a1e),
         Color(0xff144a), Color(0x0000ff), Color(0x810070), Color(0x2D8237), Color(0xffff00),
         Color(0x2e8b57), Color(0x00b6ff), Color(0x0079ff), Color(0x0057b9), Color(0x0033bd),
         Color(0xcc2ace), Color(0xff00ff), Color(0xa8ab3f), Color(0x340081), Color(0x00ff00),
         Color(0x810045), Color(0x2c1577), Color(0xffd700), Color(0x5e748c), Color(0x00ff00),
         Color(0xe09b00), Color(0x32cd50), Color(0x2e8b57), Color(0xff00ff), Color(0xffc0cb),
         Color(0x4682b4), Color(0xff69b4), Color(0x9400d3)});
  }

  // cache config
  load_config();
}

void MajorPeak::execute(Peaks &peaks) {

  if (_cfg_changed.has_value() && Config::has_changed(_cfg_changed)) {
    load_config();
    Config::want_changes(_cfg_changed);

    INFO(module_id, "EXECUTE", "config reloaded want_changes={}\n", _cfg_changed.has_value());
  }

  units.get<AcPower>(unit::AC_POWER)->on();
  units.get<DiscoBall>(unit::DISCO_BALL)->spin();

  handle_el_wire(peaks);
  handle_main_pinspot(peaks);
  handle_fill_pinspot(peaks);

  Stats::write(stats::MAX_PEAK_FREQUENCY, peaks.major_peak().frequency());
  Stats::write(stats::MAX_PEAK_MAGNITUDE, peaks.major_peak().magnitude());

  // detect if FX is in finished position (nothing is fading)
  // finished = !main->isFading() && !fill->isFading();

  finished = false;
}

void MajorPeak::handle_el_wire(Peaks &peaks) {

  // create handy array of all elwire units
  std::array elwires{units.get<ElWire>(unit::EL_DANCE), units.get<ElWire>(unit::EL_ENTRY)};

  for (auto elwire : elwires) {
    if (const auto &peak = peaks.major_peak(); peak.useable()) {

      const DutyVal x = _freq_limits.scaled_soft(). //
                        interpolate(elwire->minMaxDuty<double>(), peak.frequency().scaled());

      elwire->fixed(x);
    } else {
      elwire->dim();
    }
  }
}

void MajorPeak::handle_fill_pinspot(Peaks &peaks) {
  auto fill = units.get<PinSpot>(unit::FILL_SPOT);
  auto cfg = major_peak::find_pspot_cfg(_pspot_cfg_map, "fill pinspot");

  const auto peak = peaks.major_peak();
  if (peak.frequency() > cfg.freq_max) {
    return;
  }

  Color color = make_color(peak);

  auto start_fader = false;
  bool fading = fill->isFading();

  // when fading look for possible scenarios where the current color can be overriden
  if (fading) {
    auto freq = peak.frequency();
    auto brightness = fill->brightness();

    const auto &last_peak = _fill_last_peak;

    const auto greater_freq = cfg.when_greater.freq;
    if (freq >= greater_freq) {
      // peaks above upper bass and have a greater magnitude take priority
      // regardless of pinspot brightness
      if (peak.magnitude() > last_peak.magnitude()) {
        start_fader = true;
      }

      if (last_peak.frequency() <= greater_freq) {
        const auto bri_min = cfg.when_greater.when_higher_freq.bri_min;

        if (brightness <= bri_min) {
          start_fader = true;
        }
      }

      // anytime the pinspots brightness is low the upper bass peaks
      // take priority
      if (brightness < cfg.when_greater.bri_min) {
        start_fader = true;
      }
    }

    const auto &when_less_than = cfg.when_less_than;
    const auto lessthan_freq = when_less_than.freq;

    // bass frequencies only take priority when the pinspot's brightness has
    // reached a relatively low level
    if (freq <= lessthan_freq) {
      auto bri_min = when_less_than.bri_min;
      if (brightness <= bri_min) {
        if (color.brightness() >= brightness) {
          start_fader = true;
        }
      }
    }
  } else if (color.notBlack()) { // when not fading and it's an actual color
    start_fader = true;
  }

  if (start_fader) {
    fill->activate<FillFader>({.origin = color, .duration = cfg.fade_max});
    _fill_last_peak = peak;
  }
}

void MajorPeak::handle_main_pinspot(Peaks &peaks) {
  auto main = units.get<PinSpot>(unit::MAIN_SPOT);
  auto cfg = major_peak::find_pspot_cfg(_pspot_cfg_map, "main pinspot");

  const auto freq_min = cfg.freq_min;
  const auto peak = peaks(freq_min, Peaks::CHANNEL::RIGHT);

  if (peak.useable(_mag_limits) == false) {
    return;
  }

  Color color = make_color(peak);

  if (color.isBlack()) {
    return;
  }

  bool start_fader = false;
  bool fading = main->isFading();

  if (!fading) {
    start_fader = true;
  }

  if (fading) {
    const auto &when_fading = cfg.when_fading;

    auto brightness = main->brightness();
    auto &last_peak = _main_last_peak;

    if (peak.magnitude() >= last_peak.magnitude()) {
      start_fader = true;
    }

    if (last_peak.frequency() < peak.frequency()) {
      if (brightness < when_fading.when_freq_greater.bri_min) {
        start_fader = true;
      }
    }

    if (brightness < when_fading.bri_min) {
      start_fader = true;
    }
  }

  if (start_fader) {
    main->activate<MainFader>({.origin = color, .duration = cfg.fade_max});
    _main_last_peak = peak;
  }
}

void MajorPeak::load_config() noexcept {
  // cache the config
  _freq_limits = major_peak::cfg_freq_limits();
  _hue_cfg_map = major_peak::cfg_hue_map();
  _mag_limits = major_peak::cfg_mag_limits();
  _pspot_cfg_map = major_peak::cfg_pspot_map();

  // register for changes
  Config::want_changes(_cfg_changed);
}

const Color MajorPeak::make_color(const Peak &peak, const Color &ref) noexcept {
  auto color = ref; // initial color, may change below

  // ensure frequency can be interpolated into a color
  if (peak.useable(_mag_limits, _freq_limits.hard()) == false) {
    color = Color::black();

  } else if (peak.frequency() < _freq_limits.soft().min()) {
    // frequency less than the soft
    color.setBrightness(_mag_limits, peak.magnitude().scaled());

  } else if (peak.frequency() > _freq_limits.soft().max()) {
    auto const &hue_cfg = major_peak::find_hue_cfg(_hue_cfg_map, "above_soft_ceiling");

    auto fl_custom = freq_min_max(_freq_limits.soft().max(), _freq_limits.hard().max());
    auto hue_minmax = hue_cfg.hue_minmax();
    auto degrees = fl_custom.interpolate(hue_minmax, peak.frequency().scaled()) * hue_cfg.hue.step;

    color.rotateHue(degrees);
    if (hue_cfg.brightness.mag_scaled) {
      color.setBrightness(_mag_limits, peak.magnitude().scaled());
    } else {
      color.setBrightness(hue_cfg.brightness.max);
    }

  } else {
    const auto &hue_cfg = major_peak::find_hue_cfg(_hue_cfg_map, "generic");

    const auto fl_soft = _freq_limits.scaled_soft();
    const auto hue_min_max = hue_cfg.hue_minmax();

    auto degrees = fl_soft.interpolate(hue_min_max, peak.frequency().scaled()) * hue_cfg.hue.step;

    color.rotateHue(degrees);
    color.setBrightness(_mag_limits, peak.magnitude().scaled());
  }

  return color;
}

// must be in .cpp to avoid including Desk in .hpp
void MajorPeak::once() {
  Stats::init(); // ensure Stats object is initialized
  units.dark();
}

const Color &MajorPeak::ref_color(size_t index) const noexcept { return _ref_colors.at(index); }

// static class members
MajorPeak::ReferenceColors MajorPeak::_ref_colors;

} // namespace fx
} // namespace pierre
