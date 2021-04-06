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

#include <chrono>
#include <iostream>
#include <random>

#include <boost/format.hpp>

#include "lightdesk/faders/color/toblack.hpp"
#include "lightdesk/fx/majorpeak.hpp"
#include "misc/elapsed.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

using namespace std;
using namespace chrono;

typedef fader::color::ToBlack<fader::SimpleLinear> FillFader;
typedef fader::color::ToBlack<fader::SineDeceleratingToZero> MainFader;

MajorPeak::MajorPeak()
    : Fx(), _log(_cfg.log.file, ofstream::out | ofstream::trunc),
      _prev_peaks(88), _main_history(88), _fill_history(88) {

  std::random_device r;
  _random.seed(r());

  main = unit<PinSpot>("main");
  fill = unit<PinSpot>("fill");
  led_forest = unit<LedForest>("led forest");
  el_dance_floor = unit<ElWire>("el dance");
  el_entry = unit<ElWire>("el entry");

  // initialize static frequency to color mapping
  if (_ref_colors.size() == 0) {
    makeRefColors();
  }

  auto start_hue = 0.0f;
  if (_cfg.hue.random_start) {
    start_hue = randomHue();
    _color = Color({.hue = start_hue, .sat = 100.0f, .bri = 100.0f});
  } else {
    _color = Color({.hue = 0.0f, .sat = 100.0f, .bri = 100.0f});
  }

  _last_peak.fill = Peak::zero();
  _last_peak.main = Peak::zero();
}

void MajorPeak::execute(Peaks peaks) {
  static elapsedMillis color_elapsed;

  if (_cfg.hue.rotate && (color_elapsed > _cfg.hue.rotate_ms)) {
    _color.rotateHue(randomRotation());
    color_elapsed.reset();
  }

  handleElWire(peaks);
  handleMainPinspot(peaks);
  handleFillPinspot(peaks);

  const auto peak = peaks->majorPeak();

  _prev_peaks.push_back(peak);
}

void MajorPeak::handleElWire(Peaks peaks) {
  const auto &soft = _cfg.freq.soft;

  std::array<PulseWidthHeadUnit *, 2> elwires;
  elwires[0] = el_dance_floor.get();
  elwires[1] = el_entry.get();

  const auto freq_range = MinMaxFloat(log10(soft.floor), log10(soft.ceiling));
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

void MajorPeak::handleFillPinspot(const Peaks &peaks) {
  constexpr auto fade_duration = 500;

  const auto peak = peaks->majorPeak();
  if (peak.frequency() > 1000.0) {
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

    if (freq >= 220.0) {
      const auto &last_peak = _fill_history.front();

      // peaks above upper bass and have a greater magnitude take priority
      // regardless of pinspot brightness
      if (peak.magnitude() > last_peak.magnitude()) {
        start_fader = true;
      }

      // anytime the pinspots brightness is low the upper bass peaks
      // take priority
      if (brightness < 30.0) {
        start_fader = true;
      }
    }

    // bass frequencies only take priority when the pinspot's brightness has
    // reached a relatively low level
    if (freq <= 220.0) {
      if (brightness <= 30.0) {
        if (color.brightness() >= brightness) {
          start_fader = true;
        }
      }
    }

  } else if (color.notBlack()) { // when not fading and it's an actual color
    start_fader = true;
  }

  if (start_fader) {
    fill->activate<FillFader>({.origin = color, .ms = fade_duration});
    _fill_history.push_front(peak);
  }
}

void MajorPeak::handleMainPinspot(const Peaks peaks) {
  constexpr auto fade_duration = 500;

  auto peak = (*peaks)[180.0];

  if (useablePeak(peak) == false) {
    return;
  }

  Color color = makeColor(_color, peak);

  if (color.isBlack()) {
    return;
  }

  bool start_fader = false;
  bool fading = main->isFading();

  if (fading) {
    const auto &last_peak = _main_history.front();

    if (peak.magnitude() > last_peak.magnitude()) {
      start_fader = true;
    } else {
      auto brightness = main->brightness();

      if (brightness < 42.0) {
        if (color.brightness() > brightness) {
          start_fader = true;
        }
      }

      if (brightness < 10.0) {
        start_fader = true;
      }
    }
  } else { // not fading
    start_fader = true;
  }

  if (start_fader) {
    main->activate<MainFader>({.origin = color, .ms = fade_duration});
    _main_history.push_front(peak);
  }
}

void MajorPeak::logPeak(const Peak &peak) {
  if (_cfg.log.peak) {

    _log << boost::format("peak frequency[%7.1f] magnitude[%6.3f]\n") %
                peak.frequency() % peak.magScaled();

    _log.flush();
  }
}

void MajorPeak::logPeakColor(float low, float deg, const Peak &peak,
                             const Color &color) {
  auto freq = log10(peak.frequency());

  if (peak.frequency() >= low) {
    _log << boost::format(
                "freq[%8.2f] scaled[%4.3f] rotate[%5.3f] color %s\n") %
                peak.frequency() % freq % deg % color.asString();

    _log.flush();
  }
}

const Color MajorPeak::makeColor(Color ref, const Peak &peak) {

  const auto &hard = _cfg.freq.hard;
  const auto &soft = _cfg.freq.soft;

  const Freq freq = peak.frequency();
  bool reasonable = (freq >= hard.floor) && (freq <= hard.ceiling) &&
                    (peak.magnitude() > Peak::magFloor());

  // ensure frequency can be interpolated into a color
  if (!reasonable) {
    return move(Color::black());

  } else if (peak.frequency() < soft.floor) {
    auto color = ref;
    color.setBrightness(Peak::magScaleRange(), peak.magScaled());
    return move(color);

  } else if (peak.frequency() > soft.ceiling) {
    auto freq_range = MinMaxFloat(log10(soft.ceiling), log10(hard.ceiling));

    constexpr auto hue_step = 0.0001f;
    constexpr auto hue_min = 345.0f * (1.0f / hue_step);
    constexpr auto hue_max = 355.0f * (1.0f / hue_step);
    auto hue_range = MinMaxFloat(hue_min, hue_max);

    const float freq_scaled = log10(peak.frequency());
    float degrees = freq_range.interpolate(hue_range, freq_scaled) * hue_step;

    Color color = ref.rotateHue(degrees);
    color.setBrightness(50.0f);
    color.setBrightness(Peak::magScaleRange(), peak.magScaled());

    logPeakColor(0.0f, degrees, peak, color);
    return move(color);
  }

  // peak is good, create a color to represent it
  auto freq_range = MinMaxFloat(log10(soft.floor), log10(soft.ceiling));

  constexpr auto hue_step = 0.0001f;
  constexpr auto hue_min = 10.0f * (1.0f / hue_step);
  constexpr auto hue_max = 360.0f * (1.0f / hue_step);
  auto hue_range = MinMaxFloat(hue_min, hue_max);

  const float freq_scaled = log10(peak.frequency());
  float degrees = freq_range.interpolate(hue_range, freq_scaled) * hue_step;

  Color color = ref.rotateHue(degrees);

  color.setBrightness(Peak::magScaleRange(), peak.magScaled());

  return move(color);
}

void MajorPeak::makeRefColors() {
  _ref_colors.assign(
      {Color(0xff0000), Color(0xdc0a1e), Color(0xff002a), Color(0xb22222),
       Color(0xdc0a1e), Color(0xff144a), Color(0x0000ff), Color(0x810070),
       Color(0x2D8237), Color(0xffff00), Color(0x2e8b57), Color(0x00b6ff),
       Color(0x0079ff), Color(0x0057b9), Color(0x0033bd), Color(0xcc2ace),
       Color(0xff00ff), Color(0xa8ab3f), Color(0x340081), Color(0x00ff00),
       Color(0x810045), Color(0x2c1577), Color(0xffd700), Color(0x5e748c),
       Color(0x00ff00), Color(0xe09b00), Color(0x32cd50), Color(0x2e8b57),
       Color(0xff00ff), Color(0xffc0cb), Color(0x4682b4), Color(0xff69b4),
       Color(0x9400d3)});
}

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

  if (peak && (peak.magnitude() >= Peak::magFloor())) {
    rc = true;
  }

  return rc;
}

MajorPeak::ReferenceColors MajorPeak::_ref_colors;

} // namespace fx
} // namespace lightdesk
} // namespace pierre
