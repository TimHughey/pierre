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

#include "desk/fx/majorpeak.hpp"
#include "base/elapsed.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "desk/desk.hpp"
#include "desk/stats.hpp"
#include "desk/unit/all.hpp"
#include "fader/easings.hpp"
#include "fader/toblack.hpp"

namespace pierre {

using FillFader = fader::ToBlack<fader::SimpleLinear>;
using MainFader = fader::ToBlack<fader::SimpleLinear>;

namespace fx {

// store the shared_ptr to all the units here AND get a copy of the direct pointer
// to avoid the tax of accessing the shared_ptr
static shPinSpot sh_main;
static PinSpot *main;
static shPinSpot sh_fill;
static PinSpot *fill;
static shPulseWidth sh_led_forest;
static PulseWidth *led_forest;

MajorPeak::MajorPeak(pierre::desk::Stats &stats)
    : FX(),                     //
      _color(Hsb{0, 100, 100}), //
      stats(stats),             //
      _prev_peaks(88),          //
      _main_history(88),        //
      _fill_history(88) {

  sh_main = units->derive<PinSpot>(unit::MAIN_SPOT);
  main = sh_main.get();

  sh_fill = units->derive<PinSpot>(unit::FILL_SPOT);
  fill = sh_fill.get();

  INFO(module_id, "CONSTRUCT", "main={} fill={}\n", fmt::ptr(main), fmt::ptr(fill));

  sh_led_forest = units->derive<LedForest>(unit::LED_FOREST);
  led_forest = sh_led_forest.get();

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
}

MajorPeak::~MajorPeak() {
  sh_main.reset();
  main = nullptr;

  sh_fill.reset();
  fill = nullptr;

  sh_led_forest.reset();
  led_forest = nullptr;
}

void MajorPeak::execute(peaks_t peaks) {

  units->derive<AcPower>(unit::AC_POWER)->on();
  units->derive<DiscoBall>(unit::DISCO_BALL)->dutyPercent(0.38);

  handleElWire(peaks);
  handleMainPinspot(peaks);
  handleFillPinspot(peaks);

  _prev_peaks.push_back(peaks->majorPeak());

  stats(pierre::desk::FREQUENCY, peaks->majorPeak().frequency());
  stats(pierre::desk::MAGNITUDE, peaks->majorPeak().magnitude());

  // INFO(module_id, "DEBUG", "peak_1: {}\n", peaks->majorPeak());

  // detect if FX is in suitable finished position (nothing is fading)
  // finished = !main->isFading() && !fill->isFading();

  finished = false;
}

void MajorPeak::handleElWire(peaks_t peaks) {

  std::array elwires{units->derive<ElWire>(unit::EL_DANCE), units->derive<ElWire>(unit::EL_ENTRY)};

  for (auto elwire : elwires) {
    if (const auto &peak = peaks->majorPeak(); peak.useable()) {

      const DutyVal x =                  //
          min_max_dbl(                   //
              log10(_freq.soft.floor),   //
              log10(_freq.soft.ceiling)) //
              .interpolate(elwire->minMaxDuty<double>(), log10(peak.frequency()));

      elwire->fixed(x);
    } else {
      elwire->dim();
    }
  }
}

void MajorPeak::handleFillPinspot(peaks_t peaks) {
  const auto peak = peaks->majorPeak();
  if (peak.frequency() > _fill_spot_cfg.frequency_max) {
    return;
  }

  Color color = makeColor(_color, peak);

  auto start_fader = false;
  bool fading = fill->isFading();

  // when fading look for possible scenarios where the current color can be overriden
  if (fading) {
    auto freq = peak.frequency();
    auto brightness = fill->brightness();

    const auto &last_peak = _fill_history.front();

    const auto greater_freq = _fill_spot_cfg.when_greater.frequency;
    if (freq >= greater_freq) {
      // peaks above upper bass and have a greater magnitude take priority
      // regardless of pinspot brightness
      if (peak.magnitude() > last_peak.magnitude()) {
        start_fader = true;
      }

      if (last_peak.frequency() <= greater_freq) {
        const auto bri_min = _fill_spot_cfg.when_greater.higher_frequency.brightness_min;

        if (brightness <= bri_min) {
          start_fader = true;
        }
      }

      // anytime the pinspots brightness is low the upper bass peaks
      // take priority
      if (brightness < _fill_spot_cfg.when_greater.brightness_min) {
        start_fader = true;
      }
    }

    const auto &when_lessthan = _fill_spot_cfg.when_lessthan;
    const auto lessthan_freq = when_lessthan.frequency;

    // bass frequencies only take priority when the pinspot's brightness has
    // reached a relatively low level
    if (freq <= lessthan_freq) {
      auto bri_min = when_lessthan.brightness_min;
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
    fill->activate<FillFader>(
        {.origin = color, .duration = pet::from_ms(_fill_spot_cfg.fade_max_ms)});
    _fill_history.push_front(peak);
  }
}

void MajorPeak::handleMainPinspot(peaks_t peaks) {
  auto freq_min = _main_spot_cfg.frequency_min;
  auto peak = (*peaks)[freq_min];

  if (peak.useable() == false) {
    return;
  }

  Color color = makeColor(_color, peak);

  if (color.isBlack()) {
    return;
  }

  bool start_fader = false;
  bool fading = main->isFading();

  if (!fading) {
    start_fader = true;
  }

  if (fading) {
    const auto &when_fading = _main_spot_cfg.when_fading;

    const auto &last_peak = _main_history.front();
    auto brightness = main->brightness();

    if (peak.magnitude() >= last_peak.magnitude()) {
      start_fader = true;
    }

    if (last_peak.frequency() < peak.frequency()) {
      if (brightness < when_fading.frequency_greater.brightness_min) {
        start_fader = true;
      }
    }

    if (brightness < when_fading.brightness_min) {
      start_fader = true;
    }
  }

  if (start_fader) {
    main->activate<MainFader>({.origin = color, //
                               .duration = pet::from_ms(_main_spot_cfg.fade_max_ms)});

    _main_history.push_front(peak);
  }
}

const Color MajorPeak::makeColor(Color ref, const Peak &peak) {
  const auto hard_floor = _freq.hard.floor;
  const auto hard_ceil = _freq.hard.ceiling;

  const auto soft_floor = _freq.soft.floor;
  const auto soft_ceil = _freq.soft.ceiling;

  auto color = ref; // initial color, may change below
  bool reasonable = (peak.frequency() >= hard_floor) && (peak.frequency() <= hard_ceil) &&
                    (peak.magnitude() >= Peak::mag_base.floor);

  // ensure frequency can be interpolated into a color

  if (!reasonable) {

    color = Color::black();

  } else if (peak.frequency() < _freq.soft.floor) {
    // frequency less than the soft
    color.setBrightness(Peak::magScaleRange(), peak.magnitude().scaled());

  } else if (peak.frequency() > _freq.soft.ceiling) {
    auto const &hue_cfg = _makecolor.above_soft_ceiling.hue;

    auto const hue_min_cfg = hue_cfg.min;
    auto hue_max_cfg = hue_cfg.max;

    auto freq_range = min_max_dbl(soft_ceil, hard_ceil);

    const auto hue_step = _makecolor.above_soft_ceiling.hue.step;
    const auto hue_min = hue_min_cfg * (1.0f / hue_step);
    const auto hue_max = hue_max_cfg * (1.0f / hue_step);
    auto hue_range = min_max_dbl(hue_min, hue_max);

    auto degrees = freq_range.interpolate(hue_range, peak.frequency().scaled()) * hue_step;

    auto const &brightness = _makecolor.above_soft_ceiling.brightness;

    color = ref.rotateHue(degrees);
    color.setBrightness(brightness.max);

    if (brightness.mag_scaled) {
      color.setBrightness(Peak::magScaleRange(), peak.magnitude().scaled());
    }
  } else {
    const auto &hue_cfg = _makecolor.generic.hue;

    auto freq_range = min_max_dbl(log10(soft_floor), log10(soft_ceil));

    const auto hue_min = hue_cfg.min * (1.0f / hue_cfg.step);
    const auto hue_max = hue_cfg.max * (1.0f / hue_cfg.step);
    auto hue_range = min_max_dbl(hue_min, hue_max);

    auto degrees = freq_range.interpolate(hue_range, peak.frequency().scaled()) * hue_cfg.step;

    color = ref.rotateHue(degrees);
    color.setBrightness(Peak::magScaleRange(), peak.magnitude().scaled());
  }

  return color;
}

// must be in .cpp to avoid including Desk in .hpp
void MajorPeak::once() { units->dark(); }

Color &MajorPeak::refColor(size_t index) const { return _ref_colors.at(index); }

// static class members
MajorPeak::ReferenceColors MajorPeak::_ref_colors;

} // namespace fx
} // namespace pierre
