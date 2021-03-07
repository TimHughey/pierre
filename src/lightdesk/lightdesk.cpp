/*
    lightdesk/lightdesk.cpp - Ruth Light Desk
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

#include <math.h>

#include "lightdesk/dmx.hpp"
#include "lightdesk/lightdesk.hpp"

namespace pierre {
namespace lightdesk {

using namespace fx;

LightDesk::LightDesk() { start(); }

LightDesk::~LightDesk() {

  if (_fx != nullptr) {
    delete _fx;
    _fx = nullptr;
  }

  if (_major_peak != nullptr) {
    delete _major_peak;
    _major_peak = nullptr;
  }

  for (uint32_t i = PINSPOT_MAIN; i < PINSPOT_NONE; i++) {
    PinSpot_t *pinspot = _pinspots[i];

    if (pinspot != nullptr) {
      _pinspots[i] = nullptr;

      delete pinspot;
    }
  }

  for (uint32_t i = ELWIRE_ENTRY; i < ELWIRE_LAST; i++) {
    ElWire_t *elwire = _elwire[i];

    if (_elwire != nullptr) {
      _elwire[i] = nullptr;

      elwire->stop();

      delete elwire;
    }
  }

  if (_led_forest != nullptr) {
    delete _led_forest;
    _led_forest = nullptr;
  }

  if (_discoball != nullptr) {
    delete _discoball;
    _discoball = nullptr;
  }
}

void LightDesk::core() {

  _mode = READY;

  while (_mode != SHUTDOWN) {
    bool stream_frames = true;
    uint32_t v = 0;
    auto notified = 1;

    if (notified == 1) {
      NotifyVal_t notify = static_cast<NotifyVal_t>(v);

      PinSpot_t *pinspot = pinSpotObject(_request.func());

      // ensure DMX is streaming frames unless we're stopping or shutting down
      if ((notify == NotifyStop) || (notify == NotifyShutdown)) {
        stream_frames = false;
      }

      switch (notify) {

      case NotifyColor:
        _mode = COLOR;
        pinspot->color(_request.colorRgbw(), _request.colorStrobe());
        break;

      case NotifyDark:
        _mode = DARK;
        pinSpotMain()->dark();
        pinSpotFill()->dark();
        break;

      case NotifyDance:
        _mode = READY;
        danceStart(DANCE);
        break;

      case NotifyStop:
        stopActual();
        break;

      case NotifyShutdown:
        _mode = SHUTDOWN;
        break;

      case NotifyReady:

        _mode = READY;
        break;

      case NotifyFadeTo:
        _mode = FADE_TO;
        pinspot->fadeTo(_request.fadeToRgbw(), _request.fadeToSecs());
        break;

      case NotifyMajorPeak:
        _mode = READY;

        if (_major_peak == nullptr) {
          _major_peak = new MajorPeak();
        }

        danceStart(MAJOR_PEAK);
        break;

      case NotifyPrepareFrame:
        danceExecute();
        // _dmx->framePrepareCallback();
        break;

      default:
        stream_frames = false;
        break;
      }

      // _dmx->streamFrames(stream_frames);
    }
  }
}

void LightDesk::coreTask(void *task_instance) {
  LightDesk_t *lightdesk = (LightDesk_t *)task_instance;
  //
  // lightdesk->init();
  // lightdesk->core();
  //
  // TaskHandle_t task = lightdesk->_task.handle;
  // TR::rlog("LightDesk task %p exiting", task);
  // lightdesk->_task.handle = nullptr;
  //
  // vTaskDelete(task); // task removed from the scheduler
}

void LightDesk::danceExecute() {
  // NOTE:  when _mode is anything other than DANCE or MAJOR_PEAK this
  // function is a NOP

  if (_mode == MAJOR_PEAK) {
    _major_peak->execute();
  } else if ((_mode == DANCE) && _fx) {
    auto fx_finished = _fx->execute();

    if (fx_finished == true) {
      const auto roll = roll2D6();
      FxType_t choosen_fx = _fx_patterns[roll];

      if (_fx != nullptr) {
        FxBase_t *to_delete = _fx;
        _fx = nullptr;
        delete to_delete;
      }

      _fx = FxFactory::create(choosen_fx);
      _fx->begin();
    }
  }
}

void LightDesk::danceStart(LightDeskMode_t mode) {
  elWireDanceFloor()->dim();
  elWireEntry()->dim();
  discoball()->spin();

  pinSpotMain()->dark();
  pinSpotFill()->dark();

  // anytime the LightDesk is requested to start DANCE mode
  // delete the existing effect

  if (_fx) {
    FxBase_t *to_delete = _fx;
    _fx = nullptr;
    delete to_delete;
  }

  if (_fx == nullptr) {
    FxConfig cfg;
    cfg.i2s = _i2s;
    cfg.pinspot.main = pinSpotMain();
    cfg.pinspot.fill = pinSpotFill();
    cfg.elwire.dance_floor = elWireDanceFloor();
    cfg.elwire.entry = elWireEntry();
    cfg.led_forest = _led_forest;

    FxBase::setConfig(cfg);
  }

  Color::setScaleMinMax(50.0, 100.0);

  if (mode == DANCE) {
    FxBase::setRuntimeMax(_request.danceInterval());
  }

  _fx = new ColorBars();
  _fx->begin();

  _mode = mode;
}

void LightDesk::init() {
  //
  // _i2s = (_i2s == nullptr) ? new I2s() : _i2s;
  // _dmx = (_dmx == nullptr) ? new Dmx() : _dmx;
  //
  // PinSpot::setDmx(_dmx);
  // PulseWidthHeadUnit::setDmx(_dmx);
  //
  // _i2s->start();
  // vTaskDelay(pdMS_TO_TICKS(300)); // allow time for i2s to start
  //
  // _dmx->start();
  //
  // _ac_power = new AcPower();
  //
  // _pinspots[PINSPOT_MAIN] = new PinSpot(1); // dmx address 1
  // _pinspots[PINSPOT_FILL] = new PinSpot(7); // dmx address 7
  //
  // // pwm headunits
  // _discoball = new DiscoBall(1);               // pwm 1
  // _elwire[ELWIRE_DANCE_FLOOR] = new ElWire(2); // pwm 2
  // _elwire[ELWIRE_ENTRY] = new ElWire(3);       // pwm 3
  // _led_forest = new LedForest(4);              // pwm 4
  //
  // _dmx->prepareTaskRegister();
}

bool LightDesk::request(const Request_t &r) {
  auto rc = true;
  // _request = r;
  //
  // switch (r.mode()) {
  // case COLOR:
  //   rc = taskNotify(NotifyColor);
  //   break;
  //
  // case DANCE:
  //   rc = taskNotify(NotifyDance);
  //   break;
  //
  // case DARK:
  //   rc = taskNotify(NotifyDark);
  //   break;
  //
  // case FADE_TO:
  //   rc = taskNotify(NotifyFadeTo);
  //   break;
  //
  // case MAJOR_PEAK:
  //   rc = taskNotify(NotifyMajorPeak);
  //   break;
  //
  // case READY:
  //   rc = taskNotify(NotifyReady);
  //   break;
  //
  // case STOP:
  //   rc = taskNotify(NotifyStop);
  //   break;
  //
  // default:
  //   rc = false;
  //   break;
  // }

  return rc;
}

void LightDesk::start() {
  // if (_task.handle == nullptr) {
  //   // this (object) is passed as the data to the task creation and is
  //   // used by the static coreTask method to call cpre()
  //   ::xTaskCreate(&coreTask, "lightdesk", _task.stackSize, this,
  //   _task.priority,
  //                 &(_task.handle));
  // }
}

const LightDeskStats_t &LightDesk::stats() {
  _stats.mode = modeDesc(_mode);

  // if (_dmx) {
  //   _dmx->stats(_stats.dmx);
  // }
  //
  // if (_i2s) {
  //   _i2s->stats(_stats.i2s);
  // }

  FxBase::stats(_stats.fx);

  _stats.elwire.dance_floor = elWireDanceFloor()->duty();
  _stats.elwire.entry = elWireEntry()->duty();

  return _stats;
}

void LightDesk::stopActual() {
  LightDeskMode_t prev_mode = _mode;
  _mode = STOP;

  // _dmx->prepareTaskUnregister();

  for (uint32_t i = PINSPOT_MAIN; i < PINSPOT_NONE; i++) {
    PinSpot_t *pinspot = _pinspots[i];

    if (pinspot) {
      pinspot->dark();
    }
  }

  for (uint32_t i = ELWIRE_ENTRY; i < ELWIRE_LAST; i++) {
    ElWire_t *elwire = _elwire[i];

    if (elwire) {
      elwire->dark();
    }
  }

  _led_forest->dark();

  if (_dmx) {
    // // wait for two frames ensure any pending head unit changes are
    // reflected.
    // // the goal is a graceful and visually pleasing shutdown.
    // const TickType_t ticks = pdMS_TO_TICKS((_dmx->frameInterval() * 2) /
    // 1000); vTaskDelay(ticks); _dmx->stop();
  }

  if (_i2s) {
    // _i2s->stop();
  }

  if (discoball()) {
    discoball()->still();
  }

  // if (prev_mode != STOP) {
  //   TR::rlog("LightDesk stopped");
  // }

  // taskNotify(NotifyShutdown);
}

// bool LightDesk::taskNotify(NotifyVal_t val) const {
//   bool rc = true;
//   const uint32_t v = static_cast<uint32_t>(val);
//
//   if (task()) { // prevent attempts to notify the task that does not exist
//     xTaskNotify(task(), v, eSetValueWithOverwrite);
//   } else {
//     TR::rlog("LightDesk: attempt to send NotifyVal_t %u to null task handle",
//              val);
//     rc = false;
//   }
//
//   return rc;
// }

} // namespace lightdesk
} // namespace pierre
