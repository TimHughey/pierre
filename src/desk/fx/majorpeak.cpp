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
#include "desk/unit/all.hpp"
#include "fader/easings.hpp"
#include "fader/toblack.hpp"

#include <random>

namespace pierre {

typedef fader::ToBlack<fader::SimpleLinear> FillFader;
typedef fader::ToBlack<fader::SineDeceleratingToZero> MainFader;

namespace fx {

// store the shared_ptr to all the units here AND get a copy of the direct pointer
// to avoid the tax of accessing the shared_ptr
static shPinSpot sh_main;
static PinSpot *main;
static shPinSpot sh_fill;
static PinSpot *fill;
static shPulseWidth sh_led_forest;
static PulseWidth *led_forest;
static shPulseWidth sh_el_dance_floor;
static PulseWidth *el_dance_floor;
static shPulseWidth sh_el_entry;
static PulseWidth *el_entry;

MajorPeak::MajorPeak() : FX(), _prev_peaks(88), _main_history(88), _fill_history(88) {
  std::random_device r;
  _random.seed(r());

  sh_main = units->derive<PinSpot>(unit::MAIN_SPOT);
  main = sh_main.get();

  sh_fill = units->derive<PinSpot>(unit::FILL_SPOT);
  fill = sh_fill.get();

  sh_led_forest = units->derive<LedForest>(unit::LED_FOREST);
  led_forest = sh_led_forest.get();

  sh_el_dance_floor = units->derive<ElWire>(unit::EL_DANCE);
  el_dance_floor = sh_el_dance_floor.get();

  sh_el_entry = units->derive<ElWire>(unit::EL_ENTRY);
  el_entry = sh_el_entry.get();

  // initialize static frequency to color mapping
  if (_ref_colors.size() == 0) {
    makeRefColors();
  }

  auto start_hue = 0.0f;

  if (_color_config.random_start) {
    start_hue = randomHue();
    _color = Color({.hue = start_hue, .sat = 100.0f, .bri = 100.0f});
  } else {
    const auto &ref = _color_config.reference;

    _color = Color({.hue = ref.hue, .sat = ref.sat, .bri = ref.bri});

    _last_peak.fill = Peak::zero();
    _last_peak.main = Peak::zero();
  }
}

MajorPeak::~MajorPeak() {
  sh_main.reset();
  main = nullptr;

  sh_fill.reset();
  fill = nullptr;

  sh_led_forest.reset();
  led_forest = nullptr;

  sh_el_dance_floor.reset();
  el_dance_floor = nullptr;

  sh_el_entry.reset();
  el_entry = nullptr;
}

void MajorPeak::execute(peaks_t peaks) {
  if (_color_config.rotate.enable) {
    if (color_elapsed > _color_config.rotate.ms) {
      _color.rotateHue(randomRotation());
      color_elapsed.reset();
    }
  }

  handleElWire(peaks);
  handleMainPinspot(peaks);
  handleFillPinspot(peaks);

  _prev_peaks.push_back(peaks->majorPeak());

  // INFO(module_id, "DEBUG", "peak_1: {}\n", peaks->majorPeak());

  // detect if FX is in suitable finished position (nothing is fading)
  // finished = !main->isFading() && !fill->isFading();

  finished = false;
}

void MajorPeak::handleElWire(peaks_t peaks) {
  const auto &soft = _freq.soft;

  std::array elwires{el_dance_floor, el_entry};

  const auto freq_range = min_max_float(log10(soft.floor), log10(soft.ceiling));
  const auto &peak = peaks->majorPeak();

  if (!peak) {
    for (auto elwire : elwires) {
      elwire->dim();
    }

    return;
  }

  for (auto elwire : elwires) {
    const auto range = elwire->minMaxDuty<float>();
    const DutyVal x = freq_range.interpolate(range, log10(peak.frequency()));

    elwire->fixed(x);
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

  // when fading look for possible scenarios where the current color can
  // be overriden
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
        {.origin = color, .duration = pet::as_duration<Millis, Nanos>(_fill_spot_cfg.fade_max_ms)});
    _fill_history.push_front(peak);
  }
}

void MajorPeak::handleMainPinspot(peaks_t peaks) {
  auto freq_min = _main_spot_cfg.frequency_min;
  auto peak = (*peaks)[freq_min];

  if (useablePeak(peak) == false) {
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
    main->activate<MainFader>(
        {.origin = color, .duration = pet::as_duration<Millis, Nanos>(_main_spot_cfg.fade_max_ms)});
    _main_history.push_front(peak);
  }
}

const Color MajorPeak::makeColor(Color ref, const Peak &peak) {
  const auto hard_floor = _freq.hard.floor;
  const auto hard_ceil = _freq.hard.ceiling;

  const auto soft_floor = _freq.soft.floor;
  const auto soft_ceil = _freq.soft.ceiling;

  const Freq freq = peak.frequency();
  bool reasonable =
      (freq >= hard_floor) && (freq <= hard_ceil) && (peak.magnitude() > Peak::mag_base.floor);

  // ensure frequency can be interpolated into a color
  if (!reasonable) {
    return Color::black();
  }

  // frequency less than the soft
  if (peak.frequency() < _freq.soft.floor) {
    auto color = ref;
    color.setBrightness(Peak::magScaleRange(), peak.magScaled());
    return color;
  }

  if (peak.frequency() > _freq.soft.ceiling) {
    auto const &hue_cfg = _makecolor.above_soft_ceiling.hue;

    auto const hue_min_cfg = hue_cfg.min;
    auto hue_max_cfg = hue_cfg.max;

    auto freq_range = min_max_float(soft_ceil, hard_ceil);

    const auto hue_step = _makecolor.above_soft_ceiling.hue.step;
    const auto hue_min = hue_min_cfg * (1.0f / hue_step);
    const auto hue_max = hue_max_cfg * (1.0f / hue_step);
    auto hue_range = min_max_float(hue_min, hue_max);

    const float freq_scaled = log10(peak.frequency());
    float degrees = freq_range.interpolate(hue_range, freq_scaled) * hue_step;

    auto const &brightness = _makecolor.above_soft_ceiling.brightness;

    Color color = ref.rotateHue(degrees);
    color.setBrightness(brightness.max);

    if (brightness.mag_scaled) {
      color.setBrightness(Peak::magScaleRange(), peak.magScaled());
    }

    return color;
  }

  const auto &hue_cfg = _makecolor.generic.hue;

  auto freq_range = min_max_float(log10(soft_floor), log10(soft_ceil));

  const auto hue_min = hue_cfg.min * (1.0f / hue_cfg.step);
  const auto hue_max = hue_cfg.max * (1.0f / hue_cfg.step);
  auto hue_range = min_max_float(hue_min, hue_max);

  const float freq_scaled = log10(peak.frequency());
  float degrees = freq_range.interpolate(hue_range, freq_scaled) * hue_cfg.step;

  Color color = ref.rotateHue(degrees);

  color.setBrightness(Peak::magScaleRange(), peak.magScaled());

  return color;
}

void MajorPeak::makeRefColors() {
  _ref_colors.assign( //
      {Color(0xff0000), Color(0xdc0a1e), Color(0xff002a), Color(0xb22222), Color(0xdc0a1e),
       Color(0xff144a), Color(0x0000ff), Color(0x810070), Color(0x2D8237), Color(0xffff00),
       Color(0x2e8b57), Color(0x00b6ff), Color(0x0079ff), Color(0x0057b9), Color(0x0033bd),
       Color(0xcc2ace), Color(0xff00ff), Color(0xa8ab3f), Color(0x340081), Color(0x00ff00),
       Color(0x810045), Color(0x2c1577), Color(0xffd700), Color(0x5e748c), Color(0x00ff00),
       Color(0xe09b00), Color(0x32cd50), Color(0x2e8b57), Color(0xff00ff), Color(0xffc0cb),
       Color(0x4682b4), Color(0xff69b4), Color(0x9400d3)});
}

// must be in .cpp to avoid including Desk in .hpp
void MajorPeak::once() { units->dark(); }

double MajorPeak::randomHue() {
  auto hue = static_cast<double>(_random() % 360) / 360.0;

  return hue;
}

float MajorPeak::randomRotation() {
  auto r = static_cast<float>(_random() % 100);

  return r / 100.0f;
}

Color &MajorPeak::refColor(size_t index) const { return _ref_colors.at(index); }

bool MajorPeak::useablePeak(const Peak &peak) {
  auto rc = false;

  if (peak && (peak.magnitude() >= Peak::mag_base.floor)) {
    rc = true;
  }

  return rc;
}

MajorPeak::ReferenceColors MajorPeak::_ref_colors;

} // namespace fx
} // namespace pierre
