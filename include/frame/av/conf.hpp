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
#include "frame/fft2.hpp"
#include <base/types.hpp>

#include <fmt/format.h>
#include <iterator>

namespace pierre {
namespace frame {

struct av_conf {

  struct fft_win_conf {
    FFT2::window wt{FFT2::Hann};
    bool comp{false};

    constexpr fft_win_conf() = default;
    void assign(const toml::table &t) noexcept {
      t.for_each([this](const toml::key &k, auto &&nv) {
        if (k == "window") {
          nv.visit([this](const toml::value<string> &v) { wt = FFT2::window_lookup(v.get()); });

        } else if (k == "comp") {
          nv.visit([this](const toml::value<bool> &v) { comp = v.get(); });
        }
      });
    }
  };

  av_conf(conf::token &tokc) noexcept { load(tokc); }

  void load(conf::token &tokc) noexcept {
    init_msg.clear();

    tokc.table().for_each([this](const toml::key &key, const toml::array &arr) {
      if (key == "fft") {
        // this is the array of fft configs, iterate the array
        arr.for_each([this](const toml::table &t) {
          // within the fft configs we've found a table, iterate the table
          t.for_each([this](const toml::key &k, const toml::table &t) {
            // populate left and right based on key
            if (k == "left") {
              left.assign(t);
            } else if (k == "right") {
              right.assign(t);
            }
          });
        });
      }
    });

    auto w = std::back_inserter(init_msg);
    fmt::format_to(w, "fft left[{} {}] right[{} {}]", FFT2::win_types[left.wt], left.comp,
                   FFT2::win_types[right.wt], right.comp);
  }

  string init_msg;
  fft_win_conf left;
  fft_win_conf right;
};

} // namespace frame
} // namespace pierre