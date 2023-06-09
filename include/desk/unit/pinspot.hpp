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
#include "desk/color/hsb.hpp"
#include "desk/msg/data.hpp"
#include "desk/unit.hpp"
#include "fader/color_travel.hpp"

#include <cstdint>

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
  PinSpot(auto &&name, auto addr, size_t frame_len) noexcept
      : Unit(std::forward<decltype(name)>(name), addr, frame_len) {}
  ~PinSpot() = default;

  template <typename T> void activate(const typename T::Opts &opts) {
    fader = std::make_unique<T>(opts);
  }

  template <typename T>
    requires std::same_as<T, Hsb>
  auto &update(T &x) noexcept {
    color = x;
    return *this;
  }

  /// @brief Set the PinSpot to enable an onboard fx
  /// @param spot_fx Onboard fx to activate
  void auto_run(onboard_fx spot_fx) noexcept { fx = spot_fx; }

  auto brightness() const noexcept { return (Bri)color; }

  /// @brief Check fader procgress based on precentage complete
  /// @param percent Percentage complete to compare
  /// @return boolea, true when progress is greater than
  bool fader_progress(float percent) const noexcept { return fader->progress(percent); }

  auto &color_now() noexcept { return color; }

  void color_now(const Hsb &color_now, float strobe_val = 0.0) noexcept {
    color = color_now;

    if ((strobe_val >= 0.0) && (strobe_val <= 1.0)) {
      strobe = strobe_max * strobe;
    }

    fx = onboard_fx::None;
  }

  ///

  void dark() noexcept override {
    color.black();

    fx = onboard_fx::None;
  }

  /// @brief Prepare the PinSpot for sending the next DataMsg
  ///        This member function is an override of Unit and is
  ///        called as a calculation step.
  void prepare() noexcept override {

    // when fading we want to continue traveling via the fader
    if (fading() && fader->travel()) {
      // get the next color from the Fader
      color = fader->position();

      // ensure strobe is turned off
      strobe = 0;

    } else {
      fader.reset();
    }
  }

  /// @brief Is the PinSpot fading
  /// @return boolean, true when fasing
  bool fading() const { return (bool)fader; }

  /// @brief Update the pending DataMsg based on the PinSpot's
  ///        state as determined by the call to prepare()
  /// @param msg Reference to the DataMsg to be sent
  void update_msg(DataMsg &msg) noexcept override {
    auto snippet = msg.frame() + address;

    // byte[0] enable or disable strobe (Pinspot specific)
    *snippet++ = strobe > 0 ? strobe + 0x87 : 0xF0;

    /// NOTE:  fix me
    ///
    /// change API to accept iterator to write
    /// bytes directly into frame instead of this two
    /// step process of getting the bytes
    std::array<uint8_t, 4> color_bytes;
    color.copy_as_rgb(color_bytes);

    // bytes[1-5] rgb colors + white
    for (auto &byte : color_bytes) {
      *snippet++ = byte;
    }

    // byte[5] enable onboard fx, if set
    *snippet = fx;
  }

private:
  Hsb color;
  uint8_t strobe{0};
  uint8_t strobe_max{104};
  onboard_fx fx{onboard_fx::None};

  std::unique_ptr<Fader> fader;
  static constexpr size_t FRAME_LEN{6};

public:
  MOD_ID("pinspot");
};

} // namespace desk
} // namespace pierre
