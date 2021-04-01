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

#ifndef pierre_pinspot_device_base_hpp
#define pierre_pinspot_device_base_hpp

#include <mutex>

#include "lightdesk/color.hpp"
#include "lightdesk/faders/color.hpp"
#include "lightdesk/headunit.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

class PinSpot : public HeadUnit {
public:
  enum Fx {
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
  PinSpot(uint16_t address = 1);
  ~PinSpot() = default;

  template <typename T> void activate(const typename T::Opts &opts) {
    std::lock_guard<std::mutex> lck(_fader_mtx);

    _fader = std::make_unique<T>(opts);
  }

  void autoRun(Fx fx) { _fx = fx; }
  inline void black() { dark(); }
  bool checkFaderProgress(float percent) const;
  // const Color &color() const { return _color; }
  Color &color() { return _color; }
  void color(const Color &color, float strobe = 0.0);
  void dark() override;

  void framePrepare() override { faderMove(); }
  inline bool isFading() const { return (bool)_fader; }

  void leave() override { Color::black(); }

private:
  // functions
  void faderMove();

  void frameUpdate(dmx::Packet &packet) override;

private:
  Color _color;
  uint8_t _strobe = 0;
  uint8_t _strobe_max = 104;
  Fx _fx = Fx::None;

  std::mutex _fader_mtx;
  fader::upColor _fader;
};

typedef std::shared_ptr<PinSpot> spPinSpot;
} // namespace lightdesk
} // namespace pierre

#endif
