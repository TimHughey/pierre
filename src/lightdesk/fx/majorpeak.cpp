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

#include "lightdesk/fx/majorpeak.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

using namespace std;
using namespace chrono;
using namespace fx;

MajorPeak::MajorPeak(std::shared_ptr<audio::Dsp> dsp) : Fx(), _dsp(dsp) {

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

void MajorPeak::execute() {
  const Peak peak = _dsp->majorPeak();

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

void MajorPeak::handleLowFreq(const Peak &peak, const Color &color) {
  bool start_fade = true;

  FaderOpts_t freq_fade{.origin = color,
                        .dest = Color::black(),
                        .travel_secs = 0.7f,
                        .use_origin = true};

  const auto fading = fill->isFading();

  if (fading) {
    if ((_last_peak.fill.freq <= 180.0f) &&
        (_last_peak.fill.index == peak.index)) {
      start_fade = false;
    }
  }

  if (start_fade) {
    fill->fadeTo(freq_fade);
    _last_peak.fill = peak;
  } else if (!fading) {
    _last_peak.fill = Peak();
  }
}

void MajorPeak::handleOtherFreq(const Peak &peak, const Color &color) {
  // bool start_fade = true;
  const FaderOpts_t main_fade{.origin = color,
                              .dest = Color::black(),
                              .travel_secs = 0.7f,
                              .use_origin = true};

  const auto main_fading = main->isFading();
  const auto fill_fading = fill->isFading();

  // if (main_fading && (_last_peak.main.index == peak.index)) {
  //   start_fade = false;
  // }

  // if (start_fade) {
  //   main->fadeTo(main_fade);
  //   _last_peak.main = peak;
  // } else if (!main_fading) {
  //   _last_peak.main = Peak();
  // }

  if ((_last_peak.main.mag < peak.mag) && main_fading) {
    main->fadeTo(main_fade);
    // _last_peak.main = peak;
  } else if (!main_fading) {
    main->fadeTo(main_fade);
    // _last_peak.main = peak;
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

void MajorPeak::logPeak(const Peak &peak) const {
  if (_cfg.log) {
    array<char, 128> out;
    snprintf(out.data(), out.size(), "lightdesk mpeak mag[%12.2f] peak[%7.2f]",
             peak.mag, peak.freq);

    cerr << out.data() << endl;
  }
}

Color MajorPeak::lookupColor(const Peak &peak) {
  Color mapped_color;

  for (const FreqColor &colors : _palette) {
    const Freq_t freq = peak.freq;

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
      FreqColor{.freq = {.low = 10, .high = 60}, .color = Color::red()};

  _palette.emplace_back(first_color);

  pushPaletteColor(120, Color::fireBrick());
  pushPaletteColor(160, Color::crimson());
  pushPaletteColor(180, Color(44, 21, 119));
  pushPaletteColor(260, Color::blue());
  pushPaletteColor(300, Color::yellow75());
  pushPaletteColor(320, Color::gold());
  pushPaletteColor(350, Color::yellow());
  pushPaletteColor(390, Color(94, 116, 140)); // slate blue
  pushPaletteColor(490, Color::green());
  pushPaletteColor(550, Color(224, 155, 0)); // light orange
  pushPaletteColor(610, Color::limeGreen());
  pushPaletteColor(710, Color::seaGreen());
  pushPaletteColor(850, Color::deepPink());
  pushPaletteColor(950, Color::blueViolet());
  pushPaletteColor(1050, Color::magenta());
  pushPaletteColor(1500, Color::pink());
  pushPaletteColor(3000, Color::steelBlue());
  pushPaletteColor(5000, Color::hotPink());
  pushPaletteColor(7000, Color::darkViolet());
  pushPaletteColor(10000, Color(245, 242, 234));
  pushPaletteColor(12000, Color(245, 243, 215));
  pushPaletteColor(15000, Color(228, 228, 218));
  pushPaletteColor(22000, Color::bright());
}

void MajorPeak::pushPaletteColor(Freq_t high, const Color &color) {
  const FreqColor &last = _palette.back();
  const FreqColor &next =
      FreqColor{.freq = {.low = last.freq.high, .high = high}, .color = color};

  _palette.emplace_back(next);
}

MajorPeak::Palette MajorPeak::_palette;

} // namespace fx
} // namespace lightdesk
} // namespace pierre
