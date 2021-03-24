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

#include "lightdesk/fx/majorpeak.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

using namespace std;
using namespace chrono;

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

  FaderOpts_t freq_fade{.origin = color,
                        .dest = Color::black(),
                        .travel_secs = 0.7f,
                        .use_origin = true};

  const auto fading = fill->fader().checkProgress(75.0);

  if (fading == true) {
    if ((_last_peak.fill.freq <= 180.0f) &&
        (_last_peak.fill.index == peak.index)) {
      start_fade = false;
    }
  }

  if (start_fade) {
    fill->fadeTo(freq_fade);
  }

  _last_peak.fill = peak;
}

void MajorPeak::handleOtherFreq(const audio::Peak &peak, const Color &color) {
  // bool start_fade = true;
  const FaderOpts_t main_fade{.origin = color,
                              .dest = Color::black(),
                              .travel_secs = 0.7f,
                              .use_origin = true};

  const auto main_fading = main->fader().checkProgress(85.0);
  const auto fill_fading = fill->fader().checkProgress(75.0);

  if ((_last_peak.main.mag < peak.mag) && main_fading) {
    main->fadeTo(main_fade);
  } else if (!main_fading) {
    main->fadeTo(main_fade);
  }

  _last_peak.main = peak;

  const FaderOpts_t alt_fade{.origin = color,
                             .dest = Color::black(),
                             .travel_secs = 0.7f,
                             .use_origin = true};

  if ((_last_peak.fill.mag < peak.mag) || !fill_fading) {
    fill->fadeTo(alt_fade);
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

  pushPaletteColor(120, Color(0x8b0000)); // dark red
  pushPaletteColor(160, Color(0x752424)); //
  pushPaletteColor(180, Color(0x5b150e));
  pushPaletteColor(260, Color(0x3d6944));
  pushPaletteColor(300, Color(0xffff00));
  pushPaletteColor(320, Color(0x13497c));
  pushPaletteColor(350, Color(0xdfd43c));
  pushPaletteColor(390, Color(0x008080)); // teal
  pushPaletteColor(490, Color(0x00ff00)); // pure green
  pushPaletteColor(550, Color(0xff8c00)); // dark orange
  pushPaletteColor(610, Color(0x9bc226)); // lime green
  pushPaletteColor(710, Color(0x39737a)); // north sea green
  pushPaletteColor(850, Color(0x91234e)); //
  pushPaletteColor(950, Color(0x4c5e7c)); // violet blue
  pushPaletteColor(1050, Color(0x146e22));
  pushPaletteColor(1200, Color(0x7e104f));
  pushPaletteColor(1500, Color(0xc71585));  // medium violet red
  pushPaletteColor(3000, Color(0x8a9fbf));  // steel blue
  pushPaletteColor(5000, Color(0xff69b4));  // hot pink
  pushPaletteColor(7000, Color(0x715478));  // dark violet
  pushPaletteColor(9000, Color(0x87ceeb));  // sky blue
  pushPaletteColor(10000, Color(0x7cfc00)); // lime green
  pushPaletteColor(11000, Color(0xf8f8ff)); // ghost white
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
