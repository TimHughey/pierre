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

#include "lightdesk/faders/color/toblack.hpp"
#include "lightdesk/fx/majorpeak.hpp"
#include "misc/elapsed.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

using namespace std;
using namespace chrono;

typedef fader::color::ToBlack<fader::QuintDeceleratingToZero> Fader;

MajorPeak::MajorPeak() : Fx() {

  main = unit<PinSpot>("main");
  fill = unit<PinSpot>("fill");
  led_forest = unit<LedForest>("led forest");
  el_dance_floor = unit<ElWire>("el dance");
  el_entry = unit<ElWire>("el entry");

  // initialize static frequency to color mapping
  if (_ref_colors.size() == 0) {
    makeRefColors();
  }
}

void MajorPeak::execute(Peaks peaks) {

  handleElWire(peaks);

  const auto peak = peaks->majorPeak();

  if (peak) {
    logPeak(peak);
    const auto &ref_color = refColor(0);

    Color color = makeColor(ref_color, peak);

    if (color.notBlack()) {
      const auto range = Peak::magScaleRange();
      color.setBrightness(range, peak.magScaled());

      if (peak.freq <= 180.0) {
        handleLowFreq(peak, color);
      } else {
        handleOtherFreq(peak, color);
      }
    }

    if (peak.freq > 700.0) {
      led_forest->pulse();
    }
  }
}

void MajorPeak::handleElWire(Peaks peaks) {
  std::array<PulseWidthHeadUnit *, 2> elwires;
  elwires[0] = el_dance_floor.get();
  elwires[1] = el_entry.get();

  const auto &peak = peaks->majorPeak();

  if (peak) {
    for (auto elwire : elwires) {
      const auto range = elwire->minMaxDuty();
      const DutyVal x = peak.scaleMagToRange<DutyVal>(range);

      elwire->fixed(x);
    }
  } else {
    // zero peak, b
    for (auto elwire : elwires) {
      elwire->dim();
    }
  }
}

void MajorPeak::handleLowFreq(const Peak &peak, const Color &color) {
  bool start_fader = true;

  const auto fading = fill->checkFaderProgress(0.90);
  const auto last_mag = _last_peak.fill.mag;

  // protect from rapid repeating and overload of bass color
  if (fading && (peak.freq <= 180.0) && (last_mag > peak.mag)) {
    start_fader = false;
  }

  if (start_fader) {
    fill->activate<Fader>({.origin = color, .ms = 800});
    _last_peak.fill = peak;
  }
}

void MajorPeak::handleOtherFreq(const Peak &peak, const Color &color) {

  constexpr auto fade_duration = 700;
  const auto pmag = peak.mag;
  const auto last_mag = _last_peak.main.mag;
  const auto last_mag_plus = last_mag + (last_mag * 0.20);
  bool start_fader = main->isFading() == false;

  if ((pmag > last_mag_plus) && main->isFading()) {
    // when the current peak is larger than the previous peak by 20%
    // override the current fader
    start_fader = true;
    // } else if ((pmag > last_mag) && main->checkFaderProgress(0.19)) {
  } else if ((pmag > last_mag) && main->checkFaderProgress(0.20f)) {
    // when the current peak is simply larger than the previous peak and
    // at least 33% of the current fader has elapsed then override the
    // current fader.
    start_fader = true;
  } else if (main->checkFaderProgress(0.85f)) {
    start_fader = true;
  }

  if (start_fader) {
    _last_peak.main = peak;
    main->activate<Fader>({.origin = color, .ms = fade_duration});
  }

  if (fill->isFading() == false) {
    _last_peak.fill = Peak::zero();
  }

  if (peak.mag > _last_peak.fill.mag) {
    fill->activate<Fader>({.origin = color, .ms = fade_duration});
    _last_peak.fill = peak;
  }
}

void MajorPeak::logPeak(const Peak &peak) const {
  if (_cfg.log) {
    array<char, 128> out;
    snprintf(out.data(), out.size(), "lightdesk mpeak mag[%12.2f] peak[%7.2f]",
             peak.mag, peak.freq);

    cerr << out.data() << endl;
  }
}

const Color MajorPeak::makeColor(Color ref, const Peak &peak) {

  constexpr float freq_hard_floor = 40.0f;
  constexpr float freq_hard_ceiling = 10000.0f;
  constexpr float freq_real_min = 60.0f;
  constexpr float freq_real_max = 4000.0f;

  // ensure frequency is one to make a color for or return fixed
  // colors as needed
  if ((peak.freq < freq_hard_floor) || (peak.freq > freq_hard_ceiling)) {
    return move(Color::black());
  } else if (peak.freq < freq_real_min) {
    return move(ref);
  } else if (peak.freq > freq_real_max) {
    return move(Color::full());
  }

  constexpr float freq_min = log10(freq_real_min);
  constexpr float freq_max = log10(freq_real_max);
  auto freq_range = MinMaxFloat(freq_min, freq_max);

  constexpr auto hue_step = 0.005f;
  constexpr auto hue_max = 360.0f - 1.0e-6;
  auto hue_range = MinMaxFloat(0.0, (hue_max * (1.0f / hue_step)));

  float degrees =
      freq_range.interpolate(hue_range, log10(peak.freq)) * hue_step;

  Color color = ref.rotateHue(degrees);

  return move(color);
}

void MajorPeak::makeRefColors() {
  _ref_colors.assign(
      {Color(0xff0000), Color(0xdc0a1e), Color(0xff002a), Color(0xb22222),
       Color(0xdc0a1e), Color(0xff144a), Color(0x0000ff), Color(0x810070),
       Color(0x2D8237), Color(0xffff00), Color(0x2e8b57), Color(0x00b6ff),
       Color(0x0079ff), Color(0x0057b9), Color(0x0033bd), Color(0xcc2ace),
       Color(0xff00ff), Color(0xd7e23c), Color(0xc5c200), Color(0xa8ab3f),
       Color(0x340081), Color(0x00ff00), Color(0x810045), Color(0x2c1577),
       Color(0xbfbf00), Color(0xffd700), Color(0x5e748c), Color(0x00ff00),
       Color(0xe09b00), Color(0x32cd50), Color(0x2e8b57), Color(0x91234e),
       Color(0xff144a), Color(0xff00ff), Color(0xffc0cb), Color(0x4682b4),
       Color(0xff69b4), Color(0x9400d3), Color(0xf5f2ea), Color(0xf5f3d7),
       Color(0xe4e4d4)});
}

Color &MajorPeak::refColor(size_t index) const { return _ref_colors.at(index); }

MajorPeak::ReferenceColors MajorPeak::_ref_colors;

} // namespace fx
} // namespace lightdesk
} // namespace pierre
