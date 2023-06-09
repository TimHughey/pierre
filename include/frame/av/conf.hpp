//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/conf/token.hpp"
#include "base/conf/toml.hpp"
#include "frame/fft.hpp"
#include <base/types.hpp>

#include <fmt/format.h>
#include <iterator>

namespace pierre {
namespace frame {

struct av_conf {
  av_conf(conf::token &tokc) noexcept { load(tokc); }

  void load(conf::token &tokc) noexcept {
    init_msg.clear();

    auto w = std::back_inserter(init_msg);

    const auto vlw = tokc.val<string>("left.window", "Blackman_Nuttall");
    left.wt = FFT::window_lookup(vlw);
    tokc.val<bool>(left.comp, "left.comp", false);

    fmt::format_to(w, "left[win={} comp={}] ", vlw, left.comp);

    const auto vrw = tokc.val<string>("right.window", "Hann");
    right.wt = FFT::window_lookup(vrw);
    tokc.val<bool>(right.comp, "right.comp", false);

    fmt::format_to(w, "right[win={} comp={}]", vrw, right.comp);
  }

  string init_msg;

  struct {
    FFT::window wt{FFT::Hann};
    bool comp{false};
  } left;

  struct {
    FFT::window wt{FFT::Hann};
    bool comp{false};
  } right;
};

} // namespace frame
} // namespace pierre