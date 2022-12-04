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

#pragma once

#include "base/hard_soft_limit.hpp"
#include "base/logger.hpp"
#include "base/min_max_pair.hpp"
#include "base/types.hpp"
#include "config/config.hpp"
#include "frame/peaks/frequency.hpp"
#include "frame/peaks/magnitude.hpp"
#include "frame/peaks/types.hpp"
#include "fx/majorpeak/types.hpp"

#include <fmt/format.h>
#include <future>

namespace pierre {
namespace fx {

namespace major_peak {

inline freq_limits_t cfg_freq_limits() noexcept {
  auto freqs = Config().at("fx.majorpeak.frequencies"sv);

  return hard_soft_limit<Frequency>(freqs.at_path("hard.floor"sv).value_or(40.0),
                                    freqs.at_path("hard.ceiling"sv).value_or(11500.0),
                                    freqs.at_path("soft.floor"sv).value_or(110.0),
                                    freqs.at_path("soft.ceiling"sv).value_or(10000.0));
}

inline mag_min_max cfg_mag_limits() noexcept { // static
  auto mags = Config().at("fx.majorpeak.magnitudes"sv);

  return mag_min_max(mags.at_path("floor").value_or(2.009), mags.at_path("ceiling").value_or(64.0));
}

inline hue_cfg_map cfg_hue_map() noexcept {
  static constexpr std::array categories{csv{"generic"}, csv{"above_soft_ceiling"}};
  hue_cfg_map map;

  for (auto cat : categories) {
    const string full_path = fmt::format("fx.majorpeak.makecolors.{}", cat);

    auto cc = Config().at(full_path);

    map.try_emplace(
        string(cat),
        hue_cfg{.hue = {.min = cc.at_path("hue.min"sv).value_or(0.0),
                        .max = cc.at_path("hue.max"sv).value_or(0.0),
                        .step = cc.at_path("hue.step"sv).value_or(0.001)},
                .brightness = {.max = cc.at_path("bri.max"sv).value_or(0.0),
                               .mag_scaled = cc.at_path("hue.mag_scaled"sv).value_or(true)}});
  }

  return map;
}

inline pspot_cfg_map cfg_pspot_map() noexcept {
  static constexpr csv BRI_MIN{"bri_min"};
  major_peak::pspot_cfg_map map;

  auto mp = Config().at("fx.majorpeak"sv);
  // auto mp_pspots = toml::path("fx.majorpeak.pinspots");

  const toml::array *pspots = mp["pinspots"sv].as_array();

  for (auto &&el : *pspots) {

    auto tbl = el.as_table();

    auto entry_name = (*tbl)["name"sv].value_or(string("unnamed"));

    auto [it, inserted] = map.try_emplace(                   //
        entry_name,                                          //
        entry_name,                                          //
        (*tbl)["type"sv].value_or(string("unknown")),        //
        pet::from_ms((*tbl)["fade_max_ms"sv].value_or(100)), //
        (*tbl)["freq_min"sv].value_or(0.0),                  //
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

  return map;
}

inline Nanos cfg_silence_timeout() noexcept {
  static constexpr csv path{"fx.majorpeak.silence.timeout_ms"};
  int64_t raw_ms = Config().at(path).value_or(13000);
  return pet::from_ms<Nanos>(raw_ms);
}

inline const hue_cfg &find_hue_cfg(hue_cfg_map &map, const string cat) noexcept {
  return map.at(cat);
}

inline const pspot_cfg &find_pspot_cfg(pspot_cfg_map &map, const string name) noexcept {
  return map.at(name);
}

} // namespace major_peak

/* struct major_peak_config {

  // static major_peak::freq_limits_t freq_limits() noexcept;
  static major_peak::hue_cfg_map hue_cfg_map() noexcept;
  static major_peak::hue_cfg make_colors(csv cat) noexcept;
  static mag_min_max mag_limits() noexcept;
  static major_peak::pspot_cfg pspot(const string &name) noexcept;
  static major_peak::pspot_cfg_map pspot_map() noexcept;
  static std::shared_future<bool> want_changes() noexcept;

  static constexpr csv module_id{"MAJOR_PEAK_CFG"};
}; */

} // namespace fx
} // namespace pierre