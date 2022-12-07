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
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"
#include "config/config.hpp"
#include "desk/desk.hpp"
#include "desk/unit/all.hpp"
#include "fader/easings.hpp"
#include "fader/toblack.hpp"
#include "fx/majorpeak/types.hpp"
#include "stats/stats.hpp"

namespace pierre {

using FillFader = fader::ToBlack<fader::SimpleLinear>;
using MainFader = fader::ToBlack<fader::SimpleLinear>;

namespace fx {

namespace {
namespace stats = pierre::stats;
}

MajorPeak::MajorPeak(io_context &io_ctx) noexcept
    : FX(),                         //
      io_ctx(io_ctx),               //
      silence_timer(io_ctx),        //
      silence{false},               // has silence timeout occurred
      base_color(Hsb{0, 100, 100}), //
      _main_last_peak(),            //
      _fill_last_peak()             //
{
  // initialize static frequency to color mapping
  if (_ref_colors.size() == 0) {
    _ref_colors.assign( //
        {Color(0xff0000), Color(0xdc0a1e), Color(0xff002a), Color(0xb22222), Color(0xdc0a1e),
         Color(0xff144a), Color(0x0000ff), Color(0x810070), Color(0x2D8237), Color(0xffff00),
         Color(0x2e8b57), Color(0x00b6ff), Color(0x0079ff), Color(0x0057b9), Color(0x0033bd),
         Color(0xcc2ace), Color(0xff00ff), Color(0xa8ab3f), Color(0x340081), Color(0x00ff00),
         Color(0x810045), Color(0x2c1577), Color(0xffd700), Color(0x5e748c), Color(0x00ff00),
         Color(0xe09b00), Color(0x32cd50), Color(0x2e8b57), Color(0xff00ff), Color(0xffc0cb),
         Color(0x4682b4), Color(0xff69b4), Color(0x9400d3)});
  }

  // cache config
  load_config();
}

void MajorPeak::execute(Peaks &peaks) {

  // reset silence timer when peaks are not silent
  if (peaks.silence() == false) silence_watch();

  if (_cfg_changed.has_value() && Config::has_changed(_cfg_changed)) {
    load_config();
    silence_watch(); // restart silence watch in case silence timeout changes

    Config::want_changes(_cfg_changed);

    INFOX(module_id, "EXECUTE", "config reloaded want_changes={}\n", _cfg_changed.has_value());
  }

  units(unit_name::AC_POWER)->activate();
  units.get<Dimmable>(unit_name::DISCO_BALL)->dim();

  handle_el_wire(peaks);
  handle_main_pinspot(peaks);
  handle_fill_pinspot(peaks);

  Stats::write(stats::MAX_PEAK_FREQUENCY, peaks.major_peak().frequency());
  Stats::write(stats::MAX_PEAK_MAGNITUDE, peaks.major_peak().magnitude());

  // detect if FX is in finished position (nothing is fading) and the silence
  // timeout has expired
  set_finished((units.get<PinSpot>(unit_name::MAIN_SPOT)->isFading() == false) &&
               (units.get<PinSpot>(unit_name::FILL_SPOT)->isFading() == false) && silence.load());
}

void MajorPeak::handle_el_wire(Peaks &peaks) {

  // create handy array of all elwire units
  std::array elwires{units.get<Dimmable>(unit_name::EL_DANCE),
                     units.get<Dimmable>(unit_name::EL_ENTRY)};

  for (auto elwire : elwires) {
    if (const auto &peak = peaks.major_peak(); peak.useable()) {

      const duty_val_t x = _freq_limits.scaled_soft(). //
                           interpolate(elwire->minMaxDuty<double>(), peak.frequency().scaled());

      elwire->fixed(x);
    } else {
      elwire->dim();
    }
  }
}

void MajorPeak::handle_fill_pinspot(Peaks &peaks) {
  auto fill = units.get<PinSpot>(unit_name::FILL_SPOT);
  auto cfg = _pspot_cfg_map.at("fill pinspot");

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

    const auto &last_peak = _fill_last_peak;

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
    _fill_last_peak = peak;
  }
}

void MajorPeak::handle_main_pinspot(Peaks &peaks) {
  auto main = units.get<PinSpot>(unit_name::MAIN_SPOT);
  const auto &cfg = _pspot_cfg_map.at("main pinspot");

  const auto freq_min = cfg.freq_min;
  const auto peak = peaks(freq_min, Peaks::CHANNEL::RIGHT);

  if (peak.useable(_mag_limits) == false) {
    return;
  }

  Color color = make_color(peak);

  if (color.isBlack()) {
    return;
  }

  bool start_fader = false;
  bool fading = main->isFading();

  if (!fading) {
    start_fader = true;
  }

  if (fading) {
    const auto &when_fading = cfg.when_fading;

    auto brightness = main->brightness();
    auto &last_peak = _main_last_peak;

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
    _main_last_peak = peak;
  }
}

void MajorPeak::load_config() noexcept {
  // cache the config
  auto mp = Config().at("fx.majorpeak"sv);

  should_render = mp["will_render"sv].value_or(true);

  auto freqs = mp["frequencies"sv];
  _freq_limits = hard_soft_limit<Frequency>(freqs.at_path("hard.floor"sv).value_or(40.0),
                                            freqs.at_path("hard.ceiling"sv).value_or(11500.0),
                                            freqs.at_path("soft.floor"sv).value_or(110.0),
                                            freqs.at_path("soft.ceiling"sv).value_or(10000.0));

  // load make colors config
  for (auto cat : std::array{csv{"generic"}, csv{"above_soft_ceiling"}}) {
    const string full_path = fmt::format("fx.majorpeak.makecolors.{}", cat);

    auto cc = Config().at(full_path);

    _hue_cfg_map.try_emplace(
        string(cat),
        major_peak::hue_cfg{
            .hue = {.min = cc.at_path("hue.min"sv).value_or(0.0),
                    .max = cc.at_path("hue.max"sv).value_or(0.0),
                    .step = cc.at_path("hue.step"sv).value_or(0.001)},
            .brightness = {.max = cc.at_path("bri.max"sv).value_or(0.0),
                           .mag_scaled = cc.at_path("hue.mag_scaled"sv).value_or(true)}});
  }

  // load magnitudes
  auto mags = mp["magnitudes"sv];

  _mag_limits = mag_min_max(                 //
      mags.at_path("floor").value_or(2.009), //
      mags.at_path("ceiling").value_or(64.0));

  // load pinspot config
  static constexpr csv BRI_MIN{"bri_min"};

  for (auto &&el : *mp["pinspots"sv].as_array()) {
    auto tbl = el.as_table();

    auto entry_name = (*tbl)["name"sv].value_or(string("unnamed"));

    auto [it, inserted] = _pspot_cfg_map.try_emplace(                 //
        entry_name,                                                   //
        entry_name,                                                   //
        (*tbl)["type"sv].value_or(string("unknown")),                 //
        pet::from_val<Millis>((*tbl)["fade_max_ms"sv].value_or(100)), //
        (*tbl)["freq_min"sv].value_or(0.0),                           //
        (*tbl)["freq_max"sv].value_or(0.0));

    if (inserted) {
      auto &cfg = it->second;

      // populate 'when_less_than' (common for main and fill)
      auto wltt = (*tbl)["when_less_than"sv];
      auto &wlt = cfg.when_less_than;

      wlt.freq = wltt["freq"sv].value_or(0.0);
      wlt.bri_min = wltt[BRI_MIN].value_or(0.0);

      if (csv{cfg.type} == csv{"fill"}) { // fill specific
        auto wgt = (*tbl)["when_greater"sv];

        auto &wg = cfg.when_greater;
        wg.freq = wgt["freq"sv].value_or(0.0);
        wg.bri_min = wgt[BRI_MIN].value_or(0.0);

        auto whft = wgt["when_higher_freq"sv];
        auto &whf = wg.when_higher_freq;
        whf.bri_min = whft[BRI_MIN].value_or(0.0);

      } else if (csv(cfg.type) == csv{"main"}) { // main specific
        auto wft = (*tbl)["when_fading"sv];

        auto &wf = cfg.when_fading;
        wf.bri_min = wft[BRI_MIN].value_or(0.0);

        auto &wffg = wf.when_freq_greater;
        auto wffgt = wft["when_freq_greater"sv];
        wffg.bri_min = wffgt[BRI_MIN].value_or(0.0);
      } else {
        INFO("MAJOR_PEAK", "PINSPOT", "unrecognized type={}\n", cfg.type);
      }
    }
  }

  // load silence config
  auto st = mp["silence.timeout"sv];
  _silence_timeout = pet::from_val<Millis, Seconds>(st["seconds"].value_or(13));
  next_fx = st["suggested_fx_next"].value_or("standby");

  // register for changes
  Config::want_changes(_cfg_changed);
}

const Color MajorPeak::make_color(const Peak &peak, const Color &ref) noexcept {
  auto color = ref; // initial color, may change below

  // ensure frequency can be interpolated into a color
  if (peak.useable(_mag_limits, _freq_limits.hard()) == false) {
    color = Color::black();

  } else if (peak.frequency() < _freq_limits.soft().min()) {
    // frequency less than the soft
    color.setBrightness(_mag_limits, peak.magnitude().scaled());

  } else if (peak.frequency() > _freq_limits.soft().max()) {
    auto const &hue_cfg = _hue_cfg_map.at("above_soft_ceiling");

    auto fl_custom = freq_min_max(_freq_limits.soft().max(), _freq_limits.hard().max());
    auto hue_minmax = hue_cfg.hue_minmax();
    auto degrees = fl_custom.interpolate(hue_minmax, peak.frequency().scaled()) * hue_cfg.hue.step;

    color.rotateHue(degrees);
    if (hue_cfg.brightness.mag_scaled) {
      color.setBrightness(_mag_limits, peak.magnitude().scaled());
    } else {
      color.setBrightness(hue_cfg.brightness.max);
    }

  } else {
    const auto &hue_cfg = _hue_cfg_map.at("generic");

    const auto fl_soft = _freq_limits.scaled_soft();
    const auto hue_min_max = hue_cfg.hue_minmax();

    auto degrees = fl_soft.interpolate(hue_min_max, peak.frequency().scaled()) * hue_cfg.hue.step;

    color.rotateHue(degrees);
    color.setBrightness(_mag_limits, peak.magnitude().scaled());
  }

  return color;
}

// must be in .cpp to avoid including Desk in .hpp
void MajorPeak::once() {
  Stats::init(); // ensure Stats object is initialized

  units(unit_name::AC_POWER)->activate();
  units(unit_name::LED_FOREST)->dark();

  silence_watch();
}

const Color &MajorPeak::ref_color(size_t index) const noexcept { return _ref_colors.at(index); }

void MajorPeak::silence_watch() noexcept {

  silence_timer.expires_after(_silence_timeout);
  silence_timer.async_wait([s = ptr()](const error_code ec) {
    if (!ec) {
      s->silence.store(true);
      INFOX(module_id, "SILENCE_WATCH", "silence={}\n", s->silence.load());
    }
  });
}

// static class members
MajorPeak::ReferenceColors MajorPeak::_ref_colors;

} // namespace fx
} // namespace pierre
