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

#include "base/types.hpp"
#include "desk/color.hpp"
#include "desk/data_msg.hpp"
#include "desk/unit.hpp"
#include "fader/color_travel.hpp"

#include <cstdint>
#include <fmt/format.h>
#include <memory>

namespace pierre {

class PinSpot : public Unit, public std::enable_shared_from_this<PinSpot> {
public:
  enum FX {
    None = 0x00,
    PrimaryColorsCycle = 31,
    RedOnGreenBlueWhiteJumping = 63,
    GreenOnRedBlueWhiteJumping = 79,
    BlueOnRedGreenWhiteJumping = 95,
    WhiteOnRedGreenBlueJumping = 111,
    WhiteFadeInOut = 127,
    RgbwGradientFast = 143,
    RedGreenGradient = 159,
    RedBlueGradient = 175,
    BlueGreenGradient = 191,
    FullSpectrumCycle = 297,
    FullSpectrumJumping = 223,
    ColorCycleSound = 239,
    ColorStrobeSound = 249,
    FastStrobeSound = 254
  };

public:
  PinSpot(const auto &opts, size_t frame_len) : Unit(opts, frame_len) {}
  ~PinSpot() = default;

  template <typename T> void activate(const typename T::Opts &opts) {
    fader = std::make_unique<T>(opts);
  }

  void autoRun(FX spot_fx) { fx = spot_fx; }
  inline void black() { dark(); }
  float brightness() const { return color.brightness(); }
  bool checkFaderProgress(float percent) const { return fader->checkProgress(percent); }

  Color &colorNow() { return color; }

  void colorNow(const Color &color_now, float strobe_val = 0.0) noexcept {
    color = color_now;

    if ((strobe_val >= 0.0) && (strobe_val <= 1.0)) {
      strobe = (uint8_t)(strobe_max * strobe);
    }

    fx = FX::None;
  }

  void dark() noexcept override {
    color = Color::black();
    fx = FX::None;
  }

  void prepare() noexcept override { faderMove(); }
  inline bool isFading() const { return (bool)fader; }

  void update_msg(desk::DataMsg &msg) noexcept override {
    auto snippet = msg.dmxFrame() + address;

    color.copyRgbToByteArray(snippet + 1);

    if (strobe > 0) {
      snippet[0] = strobe + 0x87;
    } else {
      snippet[0] = 0xF0;
    }

    snippet[5] = fx;
  }

private:
  // functions
  void faderMove() noexcept {
    if (isFading()) {
      auto continue_traveling = fader->travel();
      color = fader->position();
      strobe = 0;

      if (continue_traveling == false) {
        fader.reset();
      }
    }
  }

private:
  Color color;
  uint8_t strobe{0};
  uint8_t strobe_max{104};
  FX fx{FX::None};

  std::unique_ptr<Fader> fader;
  static constexpr size_t FRAME_LEN{6};
};

} // namespace pierre
