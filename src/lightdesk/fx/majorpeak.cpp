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

#include "lightdesk/faders/toblack.hpp"
#include "lightdesk/fx/majorpeak.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

using namespace std;
using namespace chrono;

using namespace lightdesk::fader;

typedef fader::ColorToBlack<EasingOutQuint> Fader;

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

  const auto peak = peaks->majorPeak();

  if (peak) {
    logPeak(peak);

    if ((peak.freq > 220.0f) && (peak.freq < 1200.0f)) {
      el_dance_floor->pulse();
    }

    if (peak.freq > 2000.0f) {
      led_forest->pulse();
    }

    el_entry->pulse();

    Color color = lookupColor(peak);

    if (color.notBlack()) {
      color.scale(peak.magScaled());

      if (peak.freq <= 180.0) {
        handleLowFreq(peak, color);
      } else {
        handleOtherFreq(peak, color);
      }
    }
  }
}

void MajorPeak::handleLowFreq(const audio::Peak &peak, const Color &color) {
  bool start_fade = true;

  const auto fading = fill->checkFaderProgress(0.99);

  if (fading == true) {
    if ((_last_peak.fill.freq <= 180.0f) &&
        (_last_peak.fill.index == peak.index)) {
      start_fade = false;
    }
  }

  if (start_fade) {
    fill->activate<Fader>({.origin = color, .ms = 800});
    _last_peak.fill = peak;
  }
}

void MajorPeak::handleOtherFreq(const audio::Peak &peak, const Color &color) {
  const Fader::Opts main_opts({.origin = color, .ms = 700});

  if ((peak.mag > _last_peak.main.mag) && main->isFading()) {
    main->activate<Fader>(main_opts);
  } else if (main->isFading() == false) {
    main->activate<Fader>(main_opts);
  }

  _last_peak.main = peak;

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

Color MajorPeak::lookupColor(const audio::Peak &peak) {
  Color mapped_color;

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
  const FreqColor first_color = FreqColor{.freq = {.low = 10, .high = 80},
                                          .color = Color(0xff0000)}; // red

  _palette.emplace_back(first_color);

  // colors sourced from --->  https://www.easyrgb.com

  pushPaletteColor(120, Color(0xb22222)); // fire brick
  pushPaletteColor(160, Color(0xdc0a1e)); // crimson
  pushPaletteColor(180, Color(0x2c1577));
  pushPaletteColor(260, Color(0x0000ff));
  pushPaletteColor(300, Color(0xbfbf00));
  pushPaletteColor(320, Color(0xffd700));
  pushPaletteColor(350, Color(0xffff00));
  pushPaletteColor(390, Color(0x5e748c));
  pushPaletteColor(490, Color(0x00ff00)); // pure green
  pushPaletteColor(550, Color(0xe09b00));
  pushPaletteColor(610, Color(0x32cd50)); // lime green
  pushPaletteColor(710, Color(0x2e8b57));
  pushPaletteColor(850, Color(0x91234e)); //
  pushPaletteColor(950, Color(0xff144a)); // deep pink?
  pushPaletteColor(1050, Color(0xff00ff));
  pushPaletteColor(1500, Color(0xffc0cb)); // pink
  pushPaletteColor(3000, Color(0x4682b4)); // steel blue
  pushPaletteColor(5000, Color(0xff69b4)); // hot pink
  pushPaletteColor(7000, Color(0x9400d3)); // dark violet
  pushPaletteColor(10000, Color(0xf5f2ea));
  pushPaletteColor(12000, Color(0xf5f3d7));
  pushPaletteColor(15000, Color(0xe4e4d4));
  pushPaletteColor(22000, Color::full());
}

void MajorPeak::pushPaletteColor(audio::Freq_t high, const Color &color) {
  const FreqColor &last = _palette.back();
  const FreqColor &next =
      FreqColor{.freq = {.low = last.freq.high, .high = high}, .color = color};

  _palette.emplace_back(next);
}

MajorPeak::Palette MajorPeak::_palette;

} // namespace fx
} // namespace lightdesk
} // namespace pierre
