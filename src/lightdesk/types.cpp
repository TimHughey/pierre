/*
    lightdesk/types.cpp - Ruth Light Desk Types
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

#include "lightdesk/types.hpp"

namespace pierre {
namespace lightdesk {

static const char *_fx_desc[] = {"fxNone",
                                 "fxPrimaryColorsCycle",
                                 "fxRedOnGreenBlueWhiteJumping",
                                 "fxGreenOnRedBlueWhiteJumping",
                                 "fxBlueOnRedGreenWhiteJumping",
                                 "fxWhiteOnRedGreenBlueJumping",
                                 "fxWhiteFadeInOut",
                                 "fxRgbwGradientFast",
                                 "fxRedGreenGradient",
                                 "fxRedBlueGradient",
                                 "fxBlueGreenGradient",
                                 "fxFullSpectrumCycle",
                                 "fxFullSpectrumJumping",
                                 "fxColorCycleSound",
                                 "fxColorStrobeSound",
                                 "fxFastStrobeSound",
                                 "fxBeginCustom", // 0x10
                                 "fxColorBars",
                                 "fxWashedSound",
                                 "fxSimpleStrobe",
                                 "fxMajorPeak",
                                 "fxMajorPeakAlternate",
                                 "fxEndofList"};

static const char *_fx_oorange = "fxOutOfRange";

static const char *_mode_desc[] = {"invalid", "init", "color",
                                   "dance",   "dark", "fade to",
                                   "ready",   "stop", "shutdown"};

static const char *_mode_oorange = "out of range";

const char *fxDesc(const FxType_t fx) {
  if (fx < (sizeof(_fx_desc) / sizeof(const char *))) {
    return _fx_desc[fx];
  } else {
    return _fx_oorange;
  }
}

const char *modeDesc(const LightDeskMode_t mode) {
  if (mode < (sizeof(_mode_desc) / sizeof(const char *))) {
    return _mode_desc[mode];
  } else {
    return _mode_oorange;
  }
}

} // namespace lightdesk
} // namespace pierre
