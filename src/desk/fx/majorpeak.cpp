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
#include "base/elapsed.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "desk/desk.hpp"
#include "desk/unit/all.hpp"
#include "fader/easings.hpp"
#include "fader/toblack.hpp"
#include "frame/peaks.hpp"
#include "fx/majorpeak/types.hpp"
#include "lcs/config.hpp"
#include "lcs/config_watch.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"

namespace pierre {

using FillFader = fader::ToBlack<fader::SimpleLinear>;
using MainFader = fader::ToBlack<fader::SimpleLinear>;

namespace desk {

namespace {
namespace stats = pierre::stats;
}

void MajorPeak::execute(const Peaks &peaks) noexcept {
  static constexpr csv fn_id{"execute"};

  if (ConfigWatch::has_changed(cfg_changed)) {
    load_config(); // setups silence config

    cfg_changed = ConfigWatch::want_changes();
  } else if (peaks.audible()) {

    silence_watch(); // reset silence timer when we have non-silent peaks
  }

  units(unit::AC_POWER)->activate();
  units.get<Dimmable>(unit::DISCO_BALL)->dim();

  handle_el_wire(peaks);
  handle_main_pinspot(peaks);
  handle_fill_pinspot(peaks);

  // Stats::write(stats::MAX_PEAK_FREQUENCY, peaks.major_peak().frequency());
  // Stats::write(stats::MAX_PEAK_MAGNITUDE, peaks.major_peak().magnitude());
}

void MajorPeak::handle_el_wire(const Peaks &peaks) {

  // create handy array of all elwire units
  std::array elwires{units.get<Dimmable>(unit::EL_DANCE), units.get<Dimmable>(unit::EL_ENTRY)};

  for (auto elwire : elwires) {
    if (const auto &peak = peaks.major_peak(); peak.useable()) {

      const duty_val_t x = freq_limits.scaled_soft(). //
                           interpolate(elwire->minMaxDuty<double>(), peak.frequency().scaled());

      elwire->fixed(x);
    } else {
      elwire->dim();
    }
  }
}

void MajorPeak::handle_fill_pinspot(const Peaks &peaks) {
  auto fill = units.get<PinSpot>(unit::FILL_SPOT);
  auto cfg = pspot_cfg_map.at("fill pinspot");

  const auto peak = peaks.major_peak();
  if (peak.frequency() > cfg.freq_max) {
    return;
  }

  Color color = make_color(peak);

  auto start_fader = false;
  bool fading = fill->isFading();

  // when fading look for possible scenarios where the current color can be overriden
  if (fading) {
    auto freq = peak.frequency();
    auto brightness = fill->brightness();

    const auto &last_peak = fill_last_peak;

    const auto greater_freq = cfg.when_greater.freq;
    if (freq >= greater_freq) {
      // peaks above upper bass and have a greater magnitude take priority
      // regardless of pinspot brightness
      if (peak.magnitude() > last_peak.magnitude()) {
        start_fader = true;
      }

      if (last_peak.frequency() <= greater_freq) {
        const auto bri_min = cfg.when_greater.when_higher_freq.bri_min;

        if (brightness <= bri_min) {
          start_fader = true;
        }
      }

      // anytime the pinspots brightness is low the upper bass peaks
      // take priority
      if (brightness < cfg.when_greater.bri_min) {
        start_fader = true;
      }
    }

    const auto &when_less_than = cfg.when_less_than;
    const auto lessthan_freq = when_less_than.freq;

    // bass frequencies only take priority when the pinspot's brightness has
    // reached a relatively low level
    if (freq <= lessthan_freq) {
      auto bri_min = when_less_than.bri_min;
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
    fill->activate<FillFader>({.origin = color, .duration = cfg.fade_max});
    fill_last_peak = peak;
  }
}

void MajorPeak::handle_main_pinspot(const Peaks &peaks) {
  auto main = units.get<PinSpot>(unit::MAIN_SPOT);
  const auto &cfg = pspot_cfg_map.at("main pinspot");

  const auto freq_min = cfg.freq_min;
  const auto peak = peaks(freq_min, Peaks::CHANNEL::RIGHT);

  if (peak.useable(mag_limits) == false) return;

  Color color = make_color(peak);

  if (color.isBlack()) return;

  bool start_fader = false;
  bool fading = main->isFading();

  if (!fading) start_fader = true;

  if (fading) {
    const auto &when_fading = cfg.when_fading;

    auto brightness = main->brightness();
    auto &last_peak = main_last_peak;

    if (peak.magnitude() >= last_peak.magnitude()) {
      start_fader = true;
    }

    if (last_peak.frequency() < peak.frequency()) {
      if (brightness < when_fading.when_freq_greater.bri_min) {
        start_fader = true;
      }
    }

    if (brightness < when_fading.bri_min) {
      start_fader = true;
    }
  }

  if (start_fader) {
    main->activate<MainFader>({.origin = color, .duration = cfg.fade_max});
    main_last_peak = peak;
  }
}

bool MajorPeak::load_config() noexcept {
  auto changed{false};

  // make a copy of the table (under the cover of the live mtx) so
  // we are confident it isn't changing
  const toml::table local_copy(cfg_copy_live());

  // lambda helper to retrieve frequencies config
  auto freqs = [&](csv path, double def_val) {
    return local_copy[config_path<MajorPeak>("frequencies"sv).append(path)].value_or(def_val);
  };

  freq_limits = hard_soft_limit<Frequency>( //
      freqs("hard.floor"sv, 40.0),          //
      freqs("hard.ceiling"sv, 11500.0),     //
      freqs("soft.floor"sv, 110.0),         //
      freqs("soft.ceiling"sv, 10000.0));

  // load make colors config
  for (auto cat : std::array{csv{"generic"sv}, csv{"above_soft_ceiling"sv}}) {

    // lambda helper to retrieve make color configs
    auto cc = [&](csv path, auto &&def_val) {
      return local_copy[config_path<MajorPeak>("makecolors").append(cat).append(path)].value_or(
          std::forward<decltype(def_val)>(def_val));
    };

    hue_cfg_map.try_emplace(
        string(cat),
        major_peak::hue_cfg{.hue = {.min = cc("hue.min"sv, 0.0),
                                    .max = cc("hue.max"sv, 0.0),
                                    .step = cc("hue.step"sv, 0.001)},
                            .brightness = {.max = cc("bri.max"sv, 0.0),
                                           .mag_scaled = cc("hue.mag_scaled"sv, true)}});
  }

  mag_limits = mag_min_max(config_val<MajorPeak>(local_copy, "magnitudes.floor"sv, 2.009),
                           config_val<MajorPeak>(local_copy, "magnitudes.ceiling"sv, 2.009));

  // load pinspot config
  static constexpr csv BRI_MIN{"bri_min"};

  auto pspots = local_copy[config_path<MajorPeak>("pinspots"sv)];

  for (auto &&el : *pspots.as_array()) {
    // we need this twice
    const auto pspot_name = el.at_path("name"sv).value_or(string("unnamed"));

    auto [it, inserted] = //
        pspot_cfg_map.try_emplace(pspot_name, pspot_name,
                                  el.at_path("type"sv).value_or(string("unknown")),
                                  Millis(el.at_path("fade_max_ms").value_or(100)),
                                  el.at_path("freq_min"sv).value_or(0.0), //
                                  el.at_path("freq_max"sv).value_or(0.0));

    if (inserted) {
      auto &cfg = it->second;

      // populate 'when_less_than' (common for main and fill)
      auto wltt = el.at_path("when_less_than"sv);
      auto &wlt = cfg.when_less_than;

      wlt.freq = wltt["freq"sv].value_or(0.0);
      wlt.bri_min = wltt[BRI_MIN].value_or(0.0);

      if (csv{cfg.type} == csv{"fill"}) { // fill specific
        auto wgt = el.at_path("when_greater"sv);

        auto &wg = cfg.when_greater;
        wg.freq = wgt["freq"sv].value_or(0.0);
        wg.bri_min = wgt[BRI_MIN].value_or(0.0);

        auto whft = wgt["when_higher_freq"sv];
        auto &whf = wg.when_higher_freq;
        whf.bri_min = whft[BRI_MIN].value_or(0.0);

      } else if (csv(cfg.type) == csv{"main"}) { // main specific
        auto wft = el.at_path("when_fading"sv);

        auto &wf = cfg.when_fading;
        wf.bri_min = wft[BRI_MIN].value_or(0.0);

        auto &wffg = wf.when_freq_greater;
        auto wffgt = wft["when_freq_greater"sv];
        wffg.bri_min = wffgt[BRI_MIN].value_or(0.0);
      }
    }
  }

  // setup silence timeout
  const auto timeout = config_val<MajorPeak, Seconds>("silence.timeout.seconds"sv, 13);
  set_silence_timeout(timeout, fx::STANDBY);

  // register for changes
  cfg_changed = ConfigWatch::want_changes();

  return changed;
}

const Color MajorPeak::make_color(const Peak &peak, const Color &ref) noexcept {
  auto color = ref; // initial color, may change below

  // ensure frequency can be interpolated into a color
  if (peak.useable(mag_limits, freq_limits.hard()) == false) {
    color = Color::black();

  } else if (peak.frequency() < freq_limits.soft().min()) {
    // frequency less than the soft
    color.setBrightness(mag_limits, peak.magnitude().scaled());

  } else if (peak.frequency() > freq_limits.soft().max()) {
    auto const &hue_cfg = hue_cfg_map.at("above_soft_ceiling");

    auto fl_custom = freq_min_max(freq_limits.soft().max(), freq_limits.hard().max());
    auto hue_minmax = hue_cfg.hue_minmax();
    auto degrees = fl_custom.interpolate(hue_minmax, peak.frequency().scaled()) * hue_cfg.hue.step;

    color.rotateHue(degrees);
    if (hue_cfg.brightness.mag_scaled) {
      color.setBrightness(mag_limits, peak.magnitude().scaled());
    } else {
      color.setBrightness(hue_cfg.brightness.max);
    }

  } else {
    const auto &hue_cfg = hue_cfg_map.at("generic");

    const auto fl_soft = freq_limits.scaled_soft();
    const auto hue_min_max = hue_cfg.hue_minmax();

    auto degrees = fl_soft.interpolate(hue_min_max, peak.frequency().scaled()) * hue_cfg.hue.step;

    color.rotateHue(degrees);
    color.setBrightness(mag_limits, peak.magnitude().scaled());
  }

  return color;
}

const Color &MajorPeak::ref_color(size_t index) const noexcept {
  if (ref_colors.empty()) {
    ref_colors.assign( //
        {Color(0xff0000), Color(0xdc0a1e), Color(0xff002a), Color(0xb22222), Color(0xdc0a1e),
         Color(0xff144a), Color(0x0000ff), Color(0x810070), Color(0x2D8237), Color(0xffff00),
         Color(0x2e8b57), Color(0x00b6ff), Color(0x0079ff), Color(0x0057b9), Color(0x0033bd),
         Color(0xcc2ace), Color(0xff00ff), Color(0xa8ab3f), Color(0x340081), Color(0x00ff00),
         Color(0x810045), Color(0x2c1577), Color(0xffd700), Color(0x5e748c), Color(0x00ff00),
         Color(0xe09b00), Color(0x32cd50), Color(0x2e8b57), Color(0xff00ff), Color(0xffc0cb),
         Color(0x4682b4), Color(0xff69b4), Color(0x9400d3)});
  }

  return ref_colors.at(index);
}

// static class members
MajorPeak::RefColors MajorPeak::ref_colors;

} // namespace desk
} // namespace pierre
