// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "desk/fx/majorpeak.hpp"
#include "base/conf/token.hpp"
#include "base/conf/toml.hpp"
#include "base/dura.hpp"
#include "base/elapsed.hpp"
#include "base/logger.hpp"
#include "base/stats.hpp"
#include "base/types.hpp"
#include "desk/desk.hpp"
#include "desk/fader/color_travel.hpp"
#include "desk/fader/easings.hpp"
#include "desk/unit/all.hpp"
#include "frame/peaks.hpp"
#include "fx/majorpeak/conf.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace {
namespace ranges = std::ranges;
}

namespace pierre {
namespace desk {

using SpotFader = fader::ColorTravel<fader::SimpleLinear>;

MajorPeak::MajorPeak() noexcept
    : // cosntruct base FX
      FX(fx::MAJOR_PEAK, fx::STANDBY),
      // acquire a watch token
      tokc(conf::token::acquire_watch_token(module_id)),
      // create config from tokc
      runconf(std::make_unique<conf::majorpeak>(tokc)),
      // setup last peaks for both pinspots
      last_peak{Peak(), Peak()} {

  // perform the initial configuration load
  runconf->load();

  // save silence timeout so FX can track silence and auto finish
  save_silence_timeout(runconf->silence_timeout);
}

MajorPeak::~MajorPeak() noexcept { tokc->release(); }

void MajorPeak::execute(const Peaks &peaks) noexcept {
  INFO_AUTO_CAT("execute");

  if (tokc->changed()) {
    runconf->load();
    save_silence_timeout(runconf->silence_timeout);
  }

  // ensure power is on and disco ball is spun up
  unit<Switch>(unit::AC_POWER)->activate();
  unit<Dimmable>(unit::DISCO_BALL)->dim();

  // handle_el_wire(peaks);
  // handle_main_pinspot(peaks);
  // handle_fill_pinspot(peaks);

  const auto &peak = peaks();

  auto &s_specs = runconf->spot_specs;
  auto &c_specs = runconf->color_specs;

  auto scale = [](auto old_max, auto old_min, auto new_max, auto new_min, auto val) {
    // https://tinyurl.com/tlhscale

    auto old_range = old_max - old_min;
    auto new_range = new_max - new_min;

    return (((val - old_min) * new_range) / old_range) + new_min;
  };

  bool skipped{true};

  for (const auto &s : s_specs) {

    if (auto it = ranges::find(c_specs, s.color_spec, &color_spec::name); it != c_specs.end()) {

      if (peak.useable() && it->match_peak(peak)) {
        Hsb next_color = s.fade.color;

        auto bri_v = scale(-20.0, -96.0, 100.0, 0.0, (double)peak.db) * (double)next_color.bri;

        next_color.bri = Bri(bri_v);

        //  next_color.assign(peak.lerp<dB>(it->color_range<Bri>()));

        auto pinspot = unit<PinSpot>(s.unit);

        if ((pinspot->fading() == false) && next_color.visible()) {
          pinspot->activate<SpotFader>(next_color, s.fade.timeout);
        }

        next_color.write_metric();

        skipped = false;
      }

      Stats::write<MajorPeak>(stats::PEAK, peak.freq);
      Stats::write<MajorPeak>(stats::PEAK, peak.db);
    }
  }

  // Stats::write<MajorPeak>(stats::PEAK, peak.freq);
  // Stats::write<MajorPeak>(stats::PEAK, peak.db);

  if (skipped) Stats::write<MajorPeak>(stats::PEAK, 1, {"skip", "true"});
}

/*void MajorPeak::handle_el_wire(const Peaks &peaks) {

  // create handy array of all elwire units
  std::array elwires{unit<Dimmable>(unit::EL_DANCE), unit<Dimmable>(unit::EL_ENTRY)};

  for (auto elwire : elwires) {
    if (const auto &peak = peaks(); peak.useable()) {

      const duty_val x = freq_limits.scaled_soft(). //
                           interpolate(elwire->minMaxDuty<double>(), peak.freq.scaled());

      elwire->fixed(x);
    } else {
      elwire->dim();
    }
  }
}*/

/*void MajorPeak::handle_fill_pinspot(const Peaks &peaks) {

  const auto &peak1 = peaks(Peaks::Right);

  if (conf.freq_between(peak1.freq)) return;

  auto fill = unit<PinSpot>(unit::FILL_SPOT);

  bool fading = fill->fading();
  Color color = make_color(peak1);

  // always start fasing when not fading and color isn't black
  if (!fading && (bool)color) {
    last_peak[Fill] = Peak();
    fill->activate<FillFader>({.origin = color, .duration = conf.fill().fade_max});
  } else if (fading) {
    const auto &brightness = fill->brightness();
    const auto &last = last_peak[Fill];

    if ((peak1 > last) && (brightness > 30.0)) {
      last_peak[Fill] = std::move(peak1);
      fill->activate<FillFader>({.origin = color, .duration = conf.fill().fade_max});
    }
  }

        // default value for start_fader
        auto start_fader = !fading && color.notBlack();

      // when fading look for possible scenarios where the current color can be overriden
      if (!start_fader && fading) {
        auto freq = peak1.freq;
        auto brightness = fill->brightness();

        const auto &last_peak = fill_last_peak;

        const auto greater_freq = cfg.when_greater.freq;
        if (freq >= greater_freq) {
          // peaks above upper bass and have a greater magnitude take priority
          // regardless of pinspot brightness
          if (peak1.mag > last_peak.mag) {
            start_fader = true;
          }

          if (last_peak.freq <= greater_freq) {
            const auto bri_min = cfg.when_greater.when_higher_freq.bri_min;

            if (brightness <= bri_min) start_fader = true;
          }

          // anytime the frequency and brightness are less than
          // the brightness min use them
          if (brightness < cfg.when_greater.bri_min) start_fader = true;
        }

        const auto &when_less_than = cfg.when_less_than;
        const auto lessthan_freq = when_less_than.freq;

        if (freq <= lessthan_freq) { // lower frequencies take priority when
                                     // the pinspots brightness reaches a minimum brightness
          auto bri_min = when_less_than.bri_min;
          if (brightness <= bri_min) {
            if (color.brightness() >= brightness) {
              start_fader = true;
            }
          }
        }
      }

      if (start_fader) {
        fill->activate<FillFader>({.origin = color, .duration = cfg.fade_max});
        fill_last_peak = peak1;
      }
}*/

/*void MajorPeak::handle_main_pinspot(const Peaks &peaks) {
  auto main = unit<PinSpot>(unit::MAIN_SPOT);
  const auto &cfg = pspot_cfg_map.at("main pinspot");

  const auto freq_min = cfg.freq_min;
  const auto &peak = peaks(freq_min, Peaks::Left);

  if (!peak.useable(mag_limits)) return;

  Color color = make_color(peak);

  if (color.isBlack()) return;

  bool start_fader = false;
  bool fading = main->fading();

  if (!fading) start_fader = true;

  if (fading) {
    const auto &when_fading = cfg.when_fading;

    auto brightness = main->brightness();
    auto &last = last_peak[Main];

    if (peak.mag >= last.mag) start_fader = true;

    if (last.freq < peak.freq) {
      if (brightness < when_fading.when_freq_greater.bri_min) {
        start_fader = true;
      }
    }

    if (brightness < when_fading.bri_min) start_fader = true;
  }

  if (start_fader) {
    main->activate<MainFader>({.origin = color, .duration = cfg.fade_max});
    last_peak[Main] = peak;
  }
}*/

/*Hsb MajorPeak::make_color(const spot_spec &ss, const Peak &peak) noexcept {

  auto color = ref; // initial color, may change below

  constexpr auto FreqSoft = limits<Frequency>::Soft;
  constexpr auto Hard = limits_any::Hard;

  // ensure frequency can be interpolated into a color
  if (peak.useable(conf.mag_limits, conf.freq_limits)) return Color::black();

  const auto &fl = conf.freq_limits;

  if (fl.less_than(FreqSoft, peak.freq)) {
    // frequency less than the soft
    color.setBrightness(mag_limits, peak.mag.scaled());

  } else if (peak.freq > freq_limits.soft().max()) {
    auto const &hue_cfg = hue_cfg_map.at("above_soft_ceiling");

    auto fl_custom = Peaks::freq_min_max(freq_limits.soft().max(), freq_limits.hard().max());
    auto hue_minmax = hue_cfg.hue_minmax();
    auto degrees = fl_custom.interpolate(hue_minmax, peak.freq.scaled()) * hue_cfg.hue.step;

    color.rotateHue(degrees);
    if (hue_cfg.brightness.mag_scaled) {
      color.setBrightness(mag_limits, peak.mag.scaled());
    } else {
      color.setBrightness(hue_cfg.brightness.max);
    }

  } else {
    const auto &hue_cfg = hue_cfg_map.at("generic");

    const auto fl_soft = freq_limits.scaled_soft();
    const auto hue_min_max = hue_cfg.hue_minmax();

    auto degrees = fl_soft.interpolate(hue_min_max, peak.freq.scaled()) * hue_cfg.hue.step;

    color.rotateHue(degrees);
    color.setBrightness(mag_limits, peak.mag.scaled());
  }

  return color;
}*/

bool MajorPeak::once() noexcept {
  tokc->initiate_watch(); // this only needs to be done once

  unit(unit::AC_POWER)->activate();

  unit(unit::LED_FOREST)->dark();
  unit(unit::MAIN_SPOT)->dark();
  unit(unit::FILL_SPOT)->dark();

  return true;
}

const Hsb &MajorPeak::ref_color(size_t index) const noexcept {
  if (ref_colors.empty()) {
    ref_colors.assign( //
        {Hsb(0xff0000), Hsb(0xdc0a1e), Hsb(0xff002a), Hsb(0xb22222), Hsb(0xdc0a1e), Hsb(0xff144a),
         Hsb(0x0000ff), Hsb(0x810070), Hsb(0x2D8237), Hsb(0xffff00), Hsb(0x2e8b57), Hsb(0x00b6ff),
         Hsb(0x0079ff), Hsb(0x0057b9), Hsb(0x0033bd), Hsb(0xcc2ace), Hsb(0xff00ff), Hsb(0xa8ab3f),
         Hsb(0x340081), Hsb(0x00ff00), Hsb(0x810045), Hsb(0x2c1577), Hsb(0xffd700), Hsb(0x5e748c),
         Hsb(0x00ff00), Hsb(0xe09b00), Hsb(0x32cd50), Hsb(0x2e8b57), Hsb(0xff00ff), Hsb(0xffc0cb),
         Hsb(0x4682b4), Hsb(0xff69b4), Hsb(0x9400d3)});
  }

  return ref_colors.at(index);
}

// static class members
MajorPeak::RefColors MajorPeak::ref_colors;

} // namespace desk
} // namespace pierre
