/*
    lightdesk/enums.hpp - Ruth Light Desk Enumerations
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

#ifndef pierre_lightdesk_enums_hpp
#define pierre_lightdesk_enums_hpp

namespace pierre {
namespace lightdesk {
enum ColorPart {
  // used to index into a color parts array
  RED_PART = 0,
  GREEN_PART,
  BLUE_PART,
  WHITE_PART,
  END_OF_PARTS
};

enum ElWireFunction { ELWIRE_ENTRY = 0, ELWIRE_DANCE_FLOOR, ELWIRE_LAST };

enum FxType {
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

enum LightDeskMode {
  INIT = 0x01,
  COLOR,
  DANCE,
  DARK,
  FADE_TO,
  MAJOR_PEAK,
  READY,
  STOP,
  SHUTDOWN
};

enum PinSpotFunction { PINSPOT_MAIN = 0, PINSPOT_FILL, PINSPOT_NONE };

} // namespace lightdesk
} // namespace pierre

#endif
