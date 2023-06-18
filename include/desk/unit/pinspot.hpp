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

#include "base/conf/toml.hpp"
#include "base/types.hpp"
#include "desk/color/hsb.hpp"
#include "desk/fader/fader.hpp"
#include "desk/msg/data.hpp"
#include "desk/unit.hpp"

#include <cstdint>
#include <span>

namespace pierre {
namespace desk {

class PinSpot : public Unit {
public:
  enum onboard_fx {
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
  /// @brief Construct a pinspot unit
  /// @param t Reference to toml::table to use for configuration
  PinSpot(const toml::table &t) noexcept : Unit(t) {
    t["frame_len"].visit([this](const toml::value<int64_t> &v) {
      frame_len = static_cast<decltype(frame_len)>(*v);
    });
  }

  ~PinSpot() = default;

  /// @brief Set the PinSpot to enable an onboard fx
  /// @param spot_fx Onboard fx to activate
  void auto_run(onboard_fx spot_fx) noexcept { fx = spot_fx; }

  auto brightness() const noexcept { return (Bri)color; }

  auto &color_now() noexcept { return color; }

  void color_now(const Hsb &color_now, float strobe_val = 0.0) noexcept {
    color = color_now;

    if ((strobe_val >= 0.0) && (strobe_val <= 1.0)) {
      strobe = strobe_max * strobe;
    }

    fx = onboard_fx::None;
  }

  void dark() noexcept override {
    color.dark();

    fx = onboard_fx::None;
  }

  /// @brief Initiate fade from color to black
  /// @param d Duration of fade
  /// @param origin HSB representing the color to start fading from
  void initiate_fade(Nanos d, const Hsb &origin) noexcept {
    fader.initiate(d, {origin, Hsb(origin, 0.0_BRI)});
  }

  explicit operator Bri() noexcept { return (Bri)color; }
  explicit operator Hsb() noexcept { return color; }

  /// @brief Prepare the PinSpot for sending the next DataMsg
  ///        This member function is an override of Unit and is
  ///        called as a calculation step.
  void prepare() noexcept override {

    // when fading we want to continue traveling via the fader
    if (fader.active() && fader.travel()) {
      // get the next color from the Fader
      color = fader.color_now();

      // ensure strobe is turned off
      strobe = 0;
    }
  }

  /// @brief Is the PinSpot fading
  /// @return boolean, true when fading
  bool fading() const { return fader.active(); }

  /// @brief Update the pending DataMsg based on the PinSpot's
  ///        state as determined by the call to prepare()
  /// @param msg Reference to the DataMsg to be sent
  void update_msg(DataMsg &msg) noexcept override {
    auto frame{msg.frame(address, frame_len)};

    // byte[0] enable or disable strobe (Pinspot specific)
    frame[0] = strobe > 0 ? strobe + 0x87 : 0xF0;

    // bytes[1-4] color data
    color.copy_rgb_to(frame.subspan(1, 4));

    // byte[5] enable onboard fx, if set
    frame[5] = fx;
  }

private:
  Hsb color;
  uint8_t strobe{0};
  uint8_t strobe_max{104};
  onboard_fx fx{onboard_fx::None};

  Fader fader;

public:
  MOD_ID("pinspot");
};

} // namespace desk
} // namespace pierre
