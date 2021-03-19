/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#ifndef pierre_lightdesk_fx_base_hpp
#define pierre_lightdesk_fx_base_hpp

// #include "lightdesk/headunits/elwire.hpp"
// #include "lightdesk/headunits/ledforest.hpp"
#include "audio/processor/signal.hpp"
#include "lightdesk/headunits/pinspot/pinspot.hpp"
#include "lightdesk/types.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

typedef class FxBase FxBase_t;

class FxBase {
public:
  FxBase() = delete;
  FxBase(const FxType type) : _type(type) {}

  virtual ~FxBase() = default;

  virtual void begin() {}

  virtual void execute(std::shared_ptr<audio::DspResults> results) {}

  static void setFxMaxSecs(const float fx_max_secs);

  // static void stats(FxStats_t &stats) { stats = _stats; }

  FxType_t type() const { return _type; }

protected:
  FxType_t fx() const { return _type; }

  // headunits
  // inline ElWire_t *elWireDanceFloor() const { return _cfg.elwire.dance_floor;
  // } inline ElWire_t *elWireEntry() const { return _cfg.elwire.entry; } inline
  // LedForest_t *ledForest() const { return _cfg.led_forest; } inline PinSpot_t
  // *pinSpotFill() const { return _cfg.pinspot.fill; } inline PinSpot_t
  // *pinSpotMain() const { return _cfg.pinspot.main; }

  // effect runtime helpers
  void runtime(const float secs) { _runtime_secs = secs; }
  float runtimeDefault() const { return _fx_max_secs; }
  void runtimeUseDefault() { _runtime_secs = _fx_max_secs; }
  float runtimePercent(const float percent) { return _runtime_secs * percent; }
  void runtimeReduceTo(const float percent) {
    _runtime_secs = _fx_max_secs * percent;
  }

private:
  FxType_t _type = fxNone;
  // static FxConfig_t _cfg;
  // static FxStats_t _stats;

  static float _fx_max_secs;
  float _runtime_secs = _fx_max_secs;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
