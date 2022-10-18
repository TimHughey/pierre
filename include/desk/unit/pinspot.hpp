/*
    lightdesk/headunits/pinspot/base.hpp - Ruth LightDesk Headunit Pin Spot
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

#pragma once

#include "base/color.hpp"
#include "base/types.hpp"
#include "desk/data_msg.hpp"
#include "desk/unit.hpp"
#include "fader/color_travel.hpp"

#include <cstdint>
#include <fmt/format.h>
#include <memory>

namespace pierre {

class PinSpot;
typedef std::shared_ptr<PinSpot> shPinSpot;

class PinSpot : public Unit {
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
  PinSpot(const unit::Opts opts) : Unit(opts, FRAME_LEN) {}
  ~PinSpot() = default;

  template <typename T> void activate(const typename T::Opts &opts) {
    fader = std::make_unique<T>(opts);
  }

  void autoRun(FX spot_fx) { fx = spot_fx; }
  inline void black() { dark(); }
  float brightness() const { return color.brightness(); }
  bool checkFaderProgress(float percent) const { return fader->checkProgress(percent); }

  Color &colorNow() { return color; }
  void colorNow(const Color &color, float strobe = 0.0);
  void dark() override {
    color = Color::black();
    fx = FX::None;
  }

  void prepare() override { faderMove(); }
  inline bool isFading() const { return (bool)fader; }

  void leave() override { Color::black(); }

  void update_msg(desk::DataMsg &msg) override;

private:
  // functions
  void faderMove();

private:
  Color color;
  uint8_t strobe = 0;
  uint8_t strobe_max = 104;
  FX fx{FX::None};

  uqFader fader;
  static constexpr size_t FRAME_LEN = 6;
};

} // namespace pierre
