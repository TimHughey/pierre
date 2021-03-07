/*
    protocols/dmx.hpp - Ruth Dmx Protocol Engine
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

#ifndef pierre_dmx512_hpp
#define pierre_dmx512_hpp

#include "local/types.hpp"

#include "lightdesk/types.hpp"

namespace pierre {
namespace lightdesk {

typedef class Dmx Dmx_t;
typedef class DmxClient DmxClient_t;

class Dmx {

public:
  Dmx();
  ~Dmx();

  void clientRegister(DmxClient_t *client) {
    // SHORT TERM implementation, needs:
    //  1. check to prevent duplicate registrations
    //  2. unregister
    //  3. error when max clients exceeded
    if (_clients < 10) {
      _client[_clients] = client;
      _clients++;
    }
  }

  inline float fpsExpected() const {
    constexpr float seconds_us = 1000.0f * 1000.0f;
    const float frame_us = static_cast<float>(_frame_us);
    return (seconds_us / frame_us);
  }

  inline uint64_t frameInterval() const { return _frame_us; }
  inline float frameIntervalAsSeconds() {
    const float frame_us = static_cast<float>(_frame_us);
    const float frame_secs = (frame_us / (1000.0f * 1000.0f));

    return frame_secs;
  }

  float framesPerSecond() const { return _stats.fps; }

  // this member function is called by the prepareTask with the goal
  // of offloading frame preparation from the DMX task.
  void framePrepareCallback();

  inline void prepareTaskRegister() {
    // _prepare_task = xTaskGetCurrentTaskHandle();
  }

  inline void prepareTaskUnregister() {
    // _prepare_task = nullptr;
  }

  // stats
  void stats(DmxStats_t &stats) {
    stats = _stats;

    stats.frame.fps_expected = fpsExpected();
  }

  // task control
  // void start() { taskStart(); }
  // void stop() {
  //   if (_mode != STOP) {
  //     taskNotify(NotifyStop);
  //     vTaskDelay(pdMS_TO_TICKS(10)); // allow time for final frames to be
  //     sent _stats = DmxStats();
  //   }
  // }

  inline void streamFrames(bool stream = true) {
    if ((_mode != STREAM_FRAMES) && stream) {
      // taskNotify(NotifyStreamFrames);

      // allow time for initial frames to be sent
      // frameInterval() is in µs so multiple by 1000 and then by three
      // to ensure three frames are sent
      // vTaskDelay(frameInterval() * 1000.0 * 3);
    }
  }

private:
  typedef enum { INIT = 0x00, STREAM_FRAMES, PAUSE, STOP, SHUTDOWN } DmxMode_t;

private:
  inline void busyWait(uint32_t usec, const bool reset = true) {
    if (reset) {
      //_busy_wait.reset();
    }

    // while (_busy_wait <= usec) {
    //   _stats.busy_wait++;
    // yield to higher priority tasks to minimize impact of busy wait
    // taskYIELD();
    // };
  }

  static void fpsCalculate(void *data);
  void frameApplyUpdates();
  void framePrepareTaskNotify() {
    // if (_prepare_task) {
    //   xTaskNotify(_prepare_task, NotifyPrepareFrame, eSetValueWithOverwrite);
    // }
  }

  static void frameTimerCallback(void *data);
  // esp_err_t frameTimerStart();

  int txBytes();
  void txFrame();
  // esp_err_t uartInit();

  // task implementation

  // inline TaskHandle_t task() const { return _task.handle; }
  static void taskCore(void *task_instance);
  void taskInit();
  void taskLoop();
  // BaseType_t taskNotify(NotifyVal_t nval);
  void taskStart();

  void waitForStop();

private:
  // uint64_t _pin_sel = GPIO_SEL_17;
  // gpio_config_t _pin_cfg = {};
  // gpio_num_t _tx_pin = GPIO_NUM_17;
  // int _uart_num = UART_NUM_1;
  // esp_err_t _init_rc = ESP_FAIL;

  DmxMode_t _mode = INIT;
  // elapsedMicros _busy_wait;
  static const size_t _frame_len = 127;
  uint8_t _frame[_frame_len] = {}; // the DMX frame starts as all zeros
  // esp_timer_handle_t _frame_timer = nullptr;

  // except for _frame_break all frame timings are in µs
  const uint_fast32_t _frame_break = 22; // num bits at 250,000 baud (8µs)
  const uint_fast32_t _frame_mab = 12;
  const uint_fast32_t _frame_byte = 44;
  const uint_fast32_t _frame_sc = _frame_byte;
  const uint_fast32_t _frame_mtbf = 44;
  const uint_fast32_t _frame_data = _frame_byte * 512;
  // frame interval does not include the BREAK as it is handled by the UART
  uint64_t _frame_us = _frame_mab + _frame_sc + _frame_data + _frame_mtbf;
  // elapsedMicros _ftbf; // µs elapsed after frameTimerCallback invoked

  const size_t _tx_buff_len = (_frame_len < 128) ? 0 : _frame_len + 1;

  // esp_timer_handle_t _fps_timer = nullptr;
  uint64_t _frame_count_mark = 0;
  int _fpc_period = 2; // period represents seconds to count frames
  int _fpcp = 0;       // frames per calculate period

  // TaskHandle_t _prepare_task = nullptr;
  DmxClient *_client[10] = {};
  size_t _clients = 0;

  DmxStats_t _stats;

  // Task_t _task = {
  //     .handle = nullptr, .data = nullptr, .priority = 19, .stackSize = 4096};
};

class DmxClient {
public:
  DmxClient() { registerSelf(); }
  DmxClient(const uint16_t address, size_t frame_len)
      : _address(address), _frame_len(frame_len) {
    registerSelf();
  }
  ~DmxClient() {}

public:
  virtual void framePrepare() { printf("%s\n", __PRETTY_FUNCTION__); }
  virtual void frameUpdate(uint8_t *frame_actual);

  static void setDmx(Dmx_t *dmx) { DmxClient::_dmx = dmx; }

protected:
  static float fps() {
    if (_dmx) {
      return _dmx->fpsExpected();
    } else {
      return 44.0;
    }
  }
  inline bool &frameChanged() { return _frame_changed; }
  inline uint8_t *frameData() { return _frame_snippet; }
  static Dmx_t *dmx() { return _dmx; }
  void registerSelf() { _dmx->clientRegister(this); }

private:
  // class members, defined and initialized in misc/statics.cpp
  static Dmx_t *_dmx;

  uint16_t _address = 0;

  bool _frame_changed = false;
  size_t _frame_len = 0;

  uint8_t _frame_snippet[10] = {};
};
} // namespace lightdesk

} // namespace pierre

#endif
