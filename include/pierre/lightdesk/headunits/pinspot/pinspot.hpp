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

#include "lightdesk/headunit.hpp"
#include "lightdesk/headunits/pinspot/color.hpp"
#include "lightdesk/headunits/pinspot/fader.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

class PinSpot;
typedef std::shared_ptr<PinSpot> spPinSpot;

class PinSpot : public HeadUnit {
public:
  enum Fx {
    fxNone = 0x00,
    fxPrimaryColorsCycle = 0x01,
    fxRedOnGreenBlueWhiteJumping = 0x02,
    fxGreenOnRedBlueWhiteJumping = 0x03,
    fxBlueOnRedGreenWhiteJumping = 0x04,
    fxWhiteOnRedGreenBlueJumping = 0x05,
    fxWhiteFadeInOut = 0x06,
    fxRgbwGradientFast = 0x07,
    fxRedGreenGradient = 0x08,
    fxRedBlueGradient = 0x09,
    fxBlueGreenGradient = 0x0a,
    fxFullSpectrumCycle = 0x0b,
    fxFullSpectrumJumping = 0x0c,
    fxColorCycleSound = 0x0d,
    fxColorStrobeSound = 0x0e,
    fxFastStrobeSound = 0x0f,
    fxBeginCustom = 0x10,
    fxColorBars = 0x11,
    fxWashedSound,
    fxSimpleStrobe,
    fxMajorPeak,
    fxMajorPeakAlternate,
    fxEndOfList
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

  // modes
  void autoRun(Fx fx);
  inline void black() { dark(); }
  const Color &color() const { return _color; }
  void color(int r, int g, int b, int w) { color(Color(r, g, b, w)); }
  void color(const Color &color, float strobe = 0.0);

  void dark();
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
  void fadeTo(const Color &color, float secs = 1.0, float accel = 0.0);
  void fadeTo(const FaderOpts_t &opts);

  typedef enum { AUTORUN = 0x3000, DARK, COLOR, FADER } Mode_t;

private:
  // functions
  uint8_t autorunMap(Fx fx) const;

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
  Fx _fx = fxNone;

  Fader_t _fader;
}; // namespace lightdesk
} // namespace lightdesk
} // namespace pierre

#endif
