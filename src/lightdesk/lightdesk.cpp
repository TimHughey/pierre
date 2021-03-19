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
#include <math.h>

#include "lightdesk/lightdesk.hpp"

namespace pierre {
namespace lightdesk {

using namespace std;
using namespace chrono;

LightDesk::LightDesk(const Config &cfg, shared_ptr<audio::Dsp> dsp)
    : _cfg(cfg), _dsp(std::move(dsp)) {
  auto map = _hunits->map();

  map->insert(make_pair(string_t("main"), make_shared<PinSpot>(1)));
  map->insert(make_pair(string_t("fill"), make_shared<PinSpot>(7)));
  map->insert(make_pair(string_t("discoball"), make_shared<DiscoBall>(1)));
  map->insert(make_pair(string_t("el dance"), make_shared<ElWire>(2)));
  map->insert(make_pair(string_t("el entry"), make_shared<ElWire>(3)));
  map->insert(make_pair(string_t("led forest"), make_shared<LedForest>(4)));

  main = _hunits->pinspot("main");
  fill = _hunits->pinspot("fill");
  led_forest = _hunits->ledforest("led forest");
  el_dance_floor = _hunits->elWire("el dance");
  el_entry = _hunits->elWire("el entry");
  discoball = _hunits->discoball("discoball");

  main->color(Color::red());

  // initialize static frequency to color mapping
  if (_palette.size() == 0) {
    makePalette();
  }

  const auto scale = Peak::scale();

  Color::setScaleMinMax(scale.min, scale.max);

  discoball->spin();
}

void LightDesk::executeFx() {
  const Peak peak = _dsp->majorPeak();

  if (peak) {

    logPeak(peak);

    if ((peak.freq > 220.0f) && (peak.freq < 1200.0f)) {
      el_dance_floor->pulse();
    }

    if (peak.freq > 10000.0f) {
      led_forest->pulse();
    }

    el_entry->pulse();

    Color_t color = lookupColor(peak);

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

void LightDesk::handleLowFreq(const Peak &peak, const Color_t &color) {
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

void LightDesk::handleOtherFreq(const Peak &peak, const Color_t &color) {
  bool start_fade = true;
  const FaderOpts_t main_fade{.origin = color,
                              .dest = Color::black(),
                              .travel_secs = 0.7f,
                              .use_origin = true};

  const auto main_fading = main->isFading();
  const auto fill_fading = fill->isFading();

  if (main_fading && (_last_peak.main.index == peak.index)) {
    start_fade = false;
  }

  if (start_fade) {
    main->fadeTo(main_fade);
    _last_peak.main = peak;
  } else if (!main_fading) {
    _last_peak.main = Peak();
  }

  const FaderOpts_t alt_fade{.origin = color,
                             .dest = Color::black(),
                             .travel_secs = 0.7f,
                             .use_origin = true};

  if ((_last_peak.fill.mag < peak.mag) || !fill_fading) {
    fill->fadeTo(alt_fade);
    _last_peak.fill = peak;
  }
}

void LightDesk::logPeak(const Peak &peak) const {
  if (_cfg.majorpeak.log) {
    array<char, 128> out;
    snprintf(out.data(), out.size(), "lightdesk mpeak mag[%12.2f] peak[%7.2f]",
             peak.mag, peak.freq);

    cerr << out.data() << endl;
  }
}

Color_t LightDesk::lookupColor(const Peak &peak) {
  Color_t mapped_color;

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

void LightDesk::makePalette() {
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

void LightDesk::pushPaletteColor(Freq_t high, const Color_t &color) {
  const FreqColor &last = _palette.back();
  const FreqColor &next =
      FreqColor{.freq = {.low = last.freq.high, .high = high}, .color = color};

  _palette.emplace_back(next);
}

void LightDesk::prepare() {
  auto map = _hunits->map();

  for (auto t : *map) {
    auto headunit = std::get<1>(t);
    headunit->framePrepare();
  }
}

shared_ptr<thread> LightDesk::run() {
  auto t = make_shared<thread>([this]() { this->stream(); });

  return t;
}

void LightDesk::stream() {
  while (_shutdown == false) {
    this_thread::sleep_for(seconds(10));
  }
}

void LightDesk::update(dmx::UpdateInfo &info) {
  auto map = _hunits->map();

  executeFx();

  for (auto t : *map) {
    auto headunit = std::get<1>(t);

    headunit->frameUpdate(info);
  }
}

LightDesk::Palette LightDesk::_palette;

float Color::_scale_min = 50.0f;
float Color::_scale_max = 100.0f;

} // namespace lightdesk
} // namespace pierre
