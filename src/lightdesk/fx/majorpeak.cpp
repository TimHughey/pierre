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

namespace pierre {
namespace lightdesk {
namespace fx {

using namespace std;
using namespace chrono;
using namespace lightdesk::fader;

typedef color::ToBlack<EasingOutQuint> Fader;

MajorPeak::MajorPeak() : Fx() {

  main = unit<PinSpot>("main");
  fill = unit<PinSpot>("fill");
  led_forest = unit<LedForest>("led forest");
  el_dance_floor = unit<ElWire>("el dance");
  el_entry = unit<ElWire>("el entry");

  // initialize static frequency to color mapping
  if (_palette.size() == 0) {
    makePalette();
  }
}

void MajorPeak::execute(audio::spPeaks peaks) {

  if (peaks->size() == 0) {
    return;
  }

  handleElWire(peaks);

  const auto peak = peaks->peakN(2);

  if (peak) {
    logPeak(peak);

    lightdesk::Color color = lookupColor(peak);

    if (color.notBlack()) {
      color.scale(peak.magScaled());

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

void MajorPeak::handleElWire(audio::spPeaks peaks) {

  // dance floor
  for (auto p = peaks->cbegin(); p < (peaks->cbegin() + 3); p++) {

    if ((p->freq > 220.0) && (p->freq < 600)) {
      el_dance_floor->pulse();
    }

    if (p->freq > 220.0) {
      el_entry->pulse();
    }
  }
}

void MajorPeak::handleLowFreq(const audio::Peak &peak,
                              const lightdesk::Color &color) {
  bool start_fader = true;

  const auto fading = fill->checkFaderProgress(0.90);

  // if (fading == true) {
  // if ((_last_peak.fill.freq <= 180.0f) &&
  //     (_last_peak.fill.index == peak.index)) {
  //     start_fade = false;
  //   }
  // }

  const auto last_mag = _last_peak.fill.mag;

  if (fading && (peak.freq <= 180.0) && (last_mag > peak.mag)) {
    start_fader = false;
  }

  if (start_fader) {
    fill->activate<Fader>({.origin = color, .ms = 800});
    _last_peak.fill = peak;
  }
}

void MajorPeak::handleOtherFreq(const audio::Peak &peak,
                                const lightdesk::Color &color) {

  {
    const auto pmag = peak.mag;
    const auto last_mag = _last_peak.main.mag;
    const auto last_mag_plus = last_mag + (last_mag * 0.20);
    bool start_fader = main->isFading() == false;

    if ((pmag > last_mag_plus) && main->isFading()) {
      // when the current peak is larger than the previous peak by 10%
      // override the current fader
      start_fader = true;
    } else if ((pmag > last_mag) && main->checkFaderProgress(0.19)) {
      // when the current peak is simply larger than the previous peak and
      // at least 33% of the current fader has elapsed then override the
      // current fader.
      start_fader = true;
    } else if (main->checkFaderProgress(0.33)) {
      start_fader = true;
    }

    if (start_fader) {
      _last_peak.main = peak;
      main->activate<Fader>({.origin = color, .ms = 700});
    }
  }

  if (fill->isFading() == false) {
    _last_peak.fill = audio::Peak::zero();
  }

  if (peak.mag > _last_peak.fill.mag) {
    fill->activate<Fader>({.origin = color, .ms = 700});
    _last_peak.fill = peak;
  }
}

void MajorPeak::logPeak(const audio::Peak &peak) const {
  if (_cfg.log) {
    array<char, 128> out;
    snprintf(out.data(), out.size(), "lightdesk mpeak mag[%12.2f] peak[%7.2f]",
             peak.mag, peak.freq);

    cerr << out.data() << endl;
  }
}

lightdesk::Color MajorPeak::lookupColor(const audio::Peak &peak) {
  lightdesk::Color mapped_color;

  for (const FreqColor &colors : _palette) {
    const audio::Freq_t freq = peak.freq;

    if ((freq > colors.freq.low) && (freq <= colors.freq.high)) {
      mapped_color = colors.color;
    }

    if (mapped_color.notBlack()) {
      break;
    }
  }

  return mapped_color;
}

void MajorPeak::makePalette() {
  const FreqColor first_color =
      FreqColor{.freq = {.low = 10, .high = 80},
                .color = lightdesk::Color(0xff0000)}; // red

  _palette.emplace_back(first_color);

  // colors sourced from --->  https://www.easyrgb.com

  pushPaletteColor(120, lightdesk::Color(0xb22222)); // fire brick
  pushPaletteColor(160, lightdesk::Color(0xdc0a1e)); // crimson
  pushPaletteColor(180, lightdesk::Color(0x2c1577));
  pushPaletteColor(260, lightdesk::Color(0x0000ff));
  pushPaletteColor(300, lightdesk::Color(0xbfbf00));
  pushPaletteColor(320, lightdesk::Color(0xffd700));
  pushPaletteColor(350, lightdesk::Color(0xffff00));
  pushPaletteColor(390, lightdesk::Color(0x5e748c));
  pushPaletteColor(490, lightdesk::Color(0x00ff00)); // pure green
  pushPaletteColor(550, lightdesk::Color(0xe09b00));
  pushPaletteColor(610, lightdesk::Color(0x32cd50)); // lime green
  pushPaletteColor(710, lightdesk::Color(0x2e8b57));
  pushPaletteColor(850, lightdesk::Color(0x91234e)); //
  pushPaletteColor(950, lightdesk::Color(0xff144a)); // deep pink?
  pushPaletteColor(1050, lightdesk::Color(0xff00ff));
  pushPaletteColor(1500, lightdesk::Color(0xffc0cb)); // pink
  pushPaletteColor(3000, lightdesk::Color(0x4682b4)); // steel blue
  pushPaletteColor(5000, lightdesk::Color(0xff69b4)); // hot pink
  pushPaletteColor(7000, lightdesk::Color(0x9400d3)); // dark violet
  pushPaletteColor(10000, lightdesk::Color(0xf5f2ea));
  pushPaletteColor(12000, lightdesk::Color(0xf5f3d7));
  pushPaletteColor(15000, lightdesk::Color(0xe4e4d4));
  pushPaletteColor(22000, lightdesk::Color::full());
}

void MajorPeak::pushPaletteColor(audio::Freq_t high,
                                 const lightdesk::Color &color) {
  const FreqColor &last = _palette.back();
  const FreqColor &next =
      FreqColor{.freq = {.low = last.freq.high, .high = high}, .color = color};

  _palette.emplace_back(next);
}

MajorPeak::Palette MajorPeak::_palette;

} // namespace fx
} // namespace lightdesk
} // namespace pierre
