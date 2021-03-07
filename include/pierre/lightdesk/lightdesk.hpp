/*
    lightdesk/lightdesk.hpp - Ruth Light Desk
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

#ifndef pierre_lightdesk_hpp
#define pierre_lightdesk_hpp

#include "lightdesk/dmx.hpp"
#include "lightdesk/fx/factory.hpp"
#include "lightdesk/headunits/discoball.hpp"
#include "lightdesk/headunits/elwire.hpp"
#include "lightdesk/headunits/ledforest.hpp"
#include "lightdesk/headunits/pinspot/base.hpp"
#include "lightdesk/request.hpp"
#include "lightdesk/types.hpp"
#include "local/types.hpp"
#include "misc/random.hpp"

namespace pierre {
namespace lightdesk {

typedef class LightDesk LightDesk_t;

class LightDesk {

public:
  LightDesk();
  ~LightDesk();

  void *i2s() const { return nullptr; }

  static void preStart() { PulseWidthHeadUnit::preStart(); }

  bool request(const Request_t &r);

  const LightDeskStats_t &stats();

private: // headunits
  inline DiscoBall_t *discoball() { return _discoball; }
  inline ElWire_t *elWireDanceFloor() { return _elwire[ELWIRE_DANCE_FLOOR]; }
  inline ElWire_t *elWireEntry() { return _elwire[ELWIRE_ENTRY]; }

  inline PinSpot *pinSpotObject(PinSpotFunction_t func) {
    PinSpot_t *spot = nullptr;

    if (func != PINSPOT_NONE) {
      const uint32_t index = static_cast<uint32_t>(func);
      spot = _pinspots[index];
    }

    return spot;
  }

  inline PinSpot_t *pinSpotMain() { return pinSpotObject(PINSPOT_MAIN); }
  inline PinSpot_t *pinSpotFill() { return pinSpotObject(PINSPOT_FILL); }

private:
  void danceExecute();
  void danceStart(LightDeskMode_t mode);

  // bool command(MsgPayload_t &msg);

  void init();

  void majorPeakStart();

  // task
  void core();
  static void coreTask(void *task_instance);
  void start();
  void stopActual();

private:
  int _init_rc = 1;

  void *_dmx = nullptr;
  void *_i2s = nullptr;

  LightDeskMode_t _mode = INIT;
  Request_t _request = {};
  LightDeskStats_t _stats = {};

  PinSpot_t *_pinspots[2] = {};
  ElWire_t *_elwire[2] = {};
  LedForest_t *_led_forest = nullptr;
  DiscoBall_t *_discoball = nullptr;

  // map of 2d6 roll to fx patterns
  // note index 0 and 1 are impossible to roll but are included to
  // simplify the mapping and avoid index issues

  // 2d6 probabilities
  // 2: 2.78, 3: 5.56, 4: 8.33, 5: 11.11%, 6: 13.89%, 7: 16.67
  // 8: 13.89, 9: 11.11, 10: 9.33, 11: 5.56, 12: 2.78
  const FxType_t _fx_patterns[13] = {fxNone,            // 0
                                     fxNone,            // 1
                                     fxMajorPeak,       // 2
                                     fxMajorPeak,       // 3
                                     fxMajorPeak,       // 4
                                     fxMajorPeak,       // 5
                                     fxWashedSound,     // 6
                                     fxMajorPeak,       // 7
                                     fxFastStrobeSound, // 8
                                     fxMajorPeak,       // 9
                                     fxMajorPeak,       // 10
                                     fxMajorPeak,       // 11
                                     fxMajorPeak};      // 12

  const FxType_t _fx_patterns0[13] = {fxNone,       // 0
                                      fxNone,       // 1
                                      fxMajorPeak,  // 2
                                      fxMajorPeak,  // 3
                                      fxMajorPeak,  // 4
                                      fxMajorPeak,  // 5
                                      fxMajorPeak,  // 6
                                      fxMajorPeak,  // 7
                                      fxMajorPeak,  // 8
                                      fxMajorPeak,  // 9
                                      fxMajorPeak,  // 10
                                      fxMajorPeak,  // 11
                                      fxMajorPeak}; // 12

  const FxType_t _fx_patterns1[13] = {fxNone,                        // 0
                                      fxNone,                        // 1
                                      fxPrimaryColorsCycle,          // 2
                                      fxBlueGreenGradient,           // 3
                                      fxFullSpectrumCycle,           // 4
                                      fxSimpleStrobe,                // 5
                                      fxWashedSound,                 // 6
                                      fxMajorPeak,                   // 7
                                      fxFastStrobeSound,             // 8
                                      fxMajorPeak,                   // 9
                                      fxRgbwGradientFast,            // 10
                                      fxWhiteFadeInOut,              // 11
                                      fxGreenOnRedBlueWhiteJumping}; // 12

  // active Lightdesk Effect
  fx::FxBase *_fx = nullptr;
  fx::MajorPeak *_major_peak = nullptr;

  // task
  // Task_t _task = {
  //     .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
};

} // namespace lightdesk
} // namespace pierre

#endif
