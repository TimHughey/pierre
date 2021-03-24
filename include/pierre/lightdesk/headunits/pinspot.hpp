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

#include "lightdesk/color.hpp"
#include "lightdesk/fader.hpp"
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
  ~PinSpot();

  void framePrepare() override {
    if (_mode == FADER) {
      faderMove();
    }
  }

  inline bool isFading() const { return faderActive(); }
  bool nearBlack() const { return _color.nearBlack(); }

  // modes
  void autoRun(Fx fx);
  inline void black() { dark(); }
  const Color &color() const { return _color; }
  // void color(int r, int g, int b, int w) { color(Color(r, g, b, w)); }
  void color(const Color &color, float strobe = 0.0);

  void dark() override;
  inline const FaderOpts_t &fadeCurrentOpts() const {
    return _fader.initialOpts();
  }

  void fadeOut(float secs = 0.6f) {
    if (_color.notBlack()) {
      FaderOpts_t fadeout{.origin = Color::none(),
                          .dest = Color::black(),
                          .travel_secs = secs,
                          .use_origin = false};
      fadeTo(fadeout);
    }
  }

  const Fader &fader() const { return _fader; }

  void fadeTo(const Color &color, float secs = 1.0, float accel = 0.0);
  void fadeTo(const FaderOpts_t &opts);

  void leave() override { color(Color(0xff0000)); }

  typedef enum { AUTORUN = 0x3000, DARK, COLOR, FADER } Mode_t;

private:
  // functions
  inline bool faderActive() const { return _fader.active(); }
  inline bool faderFinished() const { return _fader.finished(); };
  inline const FaderOpts &faderOpts() const { return _fader.initialOpts(); }
  void faderMove();

  inline const Color &faderSelectOrigin(const FaderOpts_t &fo) const {
    if (fo.use_origin) {
      return fo.origin;
    }

    return _color;
  }

  void faderStart(const FaderOpts &opts);

  void frameUpdate(dmx::Packet &packet) override;

private:
  Mode_t _mode = DARK;

  Color _color;
  uint8_t _strobe = 0;
  uint8_t _strobe_max = 104;
  Fx _fx = Fx::None;

  Fader_t _fader;
};

typedef std::shared_ptr<PinSpot> spPinSpot;
} // namespace lightdesk
} // namespace pierre

#endif
