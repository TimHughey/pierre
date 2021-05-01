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

#include <boost/format.hpp>
#include <chrono>
#include <iostream>
#include <random>

#include "core/state.hpp"
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

auto cfg(const string_view &item) {
  auto tbl = State::config()->table()["lightdesk"sv]["majorpeak"sv];

  return std::move(tbl[item]);
}

auto cfg(const string_view &item, const string_view &sub) {
  return std::move(cfg(item)[sub]);
}

auto log_cfg(const string_view &item) {
  return std::move(cfg("logging"sv)[item]);
}
string log_file() { return log_cfg("file"sv).value_or("/dev/null"); }

auto log_filemode() {
  auto mode = ofstream::out;

  if (log_cfg("truncate"sv).value_or(false) == true) {
    mode |= ofstream::trunc;
  }

  return mode;
}

bool log(string_view category) { return log_cfg(category).value_or(false); }

MajorPeak::MajorPeak()
    : Fx(), _log(log_file(), log_filemode()), _prev_peaks(88),
      _main_history(88), _fill_history(88) {

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
  auto random_start = cfg("color"sv, "random_start"sv);
  if (random_start.value_or(false)) {
    start_hue = randomHue();
    _color = Color({.hue = start_hue, .sat = 100.0f, .bri = 100.0f});
  } else {
    auto ref = cfg("color"sv, "reference"sv);

    _color = Color({.hue = ref["hue"sv].value_or(0.0f),
                    .sat = ref["sat"sv].value_or(100.0f),
                    .bri = ref["bri"sv].value_or(100.0f)});
  }

  _last_peak.fill = Peak::zero();
  _last_peak.main = Peak::zero();
}

void MajorPeak::executeFx(Peaks peaks) {
  static elapsedMillis color_elapsed;
  auto rotate = cfg("color"sv)["rotate"];

  if (rotate["enable"sv].value_or(false)) {
    auto ms = rotate["ms"sv].value_or(7000);
    if (color_elapsed > ms) {
      _color.rotateHue(randomRotation());
      color_elapsed.reset();
    }
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
  auto fill_cfg = cfg("pinspot"sv)["fill"sv];

  const auto peak = peaks->majorPeak();
  if (peak.frequency() > fill_cfg["frequency_max"sv].value_or(1000.0)) {
    return;
  }

  auto fade_duration = fill_cfg["fade_max_ms"sv].value_or<ulong>(500);

  Color color = makeColor(_color, peak);

  auto start_fader = false;
  bool fading = fill->isFading();

  // when fading look for possible scenarios where the current color can
  // be overriden
  if (fading) {
    auto freq = peak.frequency();
    auto brightness = fill->brightness();

    auto when_greater = fill_cfg["when_greater"sv];

    const auto &last_peak = _fill_history.front();

    auto greater_freq = when_greater["frequency"sv].value_or(180.0);
    if (freq >= greater_freq) {

      // peaks above upper bass and have a greater magnitude take priority
      // regardless of pinspot brightness
      if (peak.magnitude() > last_peak.magnitude()) {
        start_fader = true;
      }

      if (last_peak.frequency() <= greater_freq) {
        auto higher_frequency = when_greater["higher_frequency"sv];
        auto bri_min = higher_frequency["brightness_min"sv].value_or(0.0);
        if (brightness <= bri_min) {
          start_fader = true;
        }
      }

      // anytime the pinspots brightness is low the upper bass peaks
      // take priority
      if (brightness < when_greater["brightness_min"].value_or(0.0)) {
        start_fader = true;
      }
    }

    auto when_lessthan = fill_cfg["when_lessthan"sv];
    auto lessthan_freq = when_lessthan["frequency"sv].value_or(180.0);

    // bass frequencies only take priority when the pinspot's brightness has
    // reached a relatively low level
    if (freq <= lessthan_freq) {
      auto bri_min = when_lessthan["brightness_min"sv].value_or(0.0);
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
    fill->activate<FillFader>({.origin = color, .ms = fade_duration});
    _fill_history.push_front(peak);
  }
}

void MajorPeak::handleMainPinspot(const Peaks peaks) {
  auto main_cfg = cfg("pinspot"sv)["main"sv];

  auto fade_duration = main_cfg["fade_max_ms"sv].value_or<ulong>(700);

  auto freq_min = main_cfg["frequency_min"sv].value_or(180.0);
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
    auto when_fading = main_cfg["when_fading"sv];

    const auto &last_peak = _main_history.front();
    auto brightness = main->brightness();

    if (peak.magnitude() >= last_peak.magnitude()) {
      start_fader = true;
    }

    if (last_peak.frequency() < peak.frequency()) {
      auto freq_greater = when_fading["freq_greater"sv];

      if (brightness < freq_greater["brightness_min"].value_or(40.0)) {
        start_fader = true;
      }
    }

    if (brightness < when_fading["brightness_min"sv].value_or(5.0)) {
      start_fader = true;
    }
  }

  if (start_fader) {
    main->activate<MainFader>({.origin = color, .ms = fade_duration});
    _main_history.push_front(peak);
  }
}

void MajorPeak::logPeak(const Peak &peak) {
  if (log("peaks"sv)) {

    _log << boost::format("peak frequency[%7.1f] magnitude[%6.3f]\n") %
                peak.frequency() % peak.magScaled();

    _log.flush();
  }
}

void MajorPeak::logPeakColor(float low, float deg, const Peak &peak,
                             const Color &color) {

  if (log("colors"sv)) {
    auto freq = log10(peak.frequency());

    if (peak.frequency() >= low) {
      _log << boost::format(
                  "freq[%8.2f] scaled[%4.3f] rotate[%5.3f] color %s\n") %
                  peak.frequency() % freq % deg % color.asString();

      _log.flush();
    }
  }
}

const Color MajorPeak::makeColor(Color ref, const Peak &peak) {

  auto frequencies = cfg("frequencies"sv);
  auto hard = frequencies["hard"sv];
  auto hard_floor = hard["floor"sv].value_or(40.0f);
  auto hard_ceil = hard["ceiling"sv].value_or(10000.0f);

  const Freq freq = peak.frequency();
  bool reasonable = (freq >= hard_floor) && (freq <= hard_ceil) &&
                    (peak.magnitude() > Peak::magFloor());

  // ensure frequency can be interpolated into a color
  if (!reasonable) {
    return move(Color::black());
  }

  auto soft = frequencies["soft"sv];
  auto soft_floor = soft["floor"sv].value_or(110.0f);
  auto soft_ceil = soft["ceiling"sv].value_or(1500.0f);

  // frequency less than the soft
  if (peak.frequency() < soft_floor) {
    auto color = ref;
    color.setBrightness(Peak::magScaleRange(), peak.magScaled());
    return move(color);
  }

  auto color_cfg = cfg("makecolor"sv);

  if (peak.frequency() > soft_ceil) {
    auto cfg = color_cfg["above_soft_ceiling"sv];
    auto hue = cfg["hue"sv];
    auto hue_min_cfg = hue["min"sv].value_or(345.0f);
    auto hue_max_cfg = hue["max"sv].value_or(355.0f);

    auto freq_range = MinMaxFloat(soft_ceil, hard_ceil);

    const auto hue_step = hue["step"sv].value_or(0.0001f);
    const auto hue_min = hue_min_cfg * (1.0f / hue_step);
    const auto hue_max = hue_max_cfg * (1.0f / hue_step);
    auto hue_range = MinMaxFloat(hue_min, hue_max);

    const float freq_scaled = log10(peak.frequency());
    float degrees = freq_range.interpolate(hue_range, freq_scaled) * hue_step;

    auto brightness = cfg["brightness"sv];

    Color color = ref.rotateHue(degrees);
    color.setBrightness(brightness["max"sv].value_or(50.0f));

    if (brightness["mag_scaled"sv].value_or(true)) {
      color.setBrightness(Peak::magScaleRange(), peak.magScaled());
    }

    logPeakColor(0.0f, degrees, peak, color);
    return move(color);
  }

  auto cfg = color_cfg["generic"sv];
  auto hue = cfg["hue"sv];
  auto hue_min_cfg = hue["min"sv].value_or(10.0f);
  auto hue_max_cfg = hue["max"sv].value_or(360.0f);

  auto freq_range = MinMaxFloat(log10(soft_floor), log10(soft_ceil));

  const auto hue_step = hue["step"sv].value_or(0.0001f);
  const auto hue_min = hue_min_cfg * (1.0f / hue_step);
  const auto hue_max = hue_max_cfg * (1.0f / hue_step);
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

void MajorPeak::once() {
  unit<LedForest>("led forest")->dark();
  unit<ElWire>("el dance")->dark();
  unit<ElWire>("el entry")->dark();
  unit<DiscoBall>("discoball")->spin();

  main->black();
  fill->black();
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
