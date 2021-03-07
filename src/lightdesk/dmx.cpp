/*
     protocols/dmx.cpp - Ruth Dmx Protocol Engine
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

#include "lightdesk/dmx.hpp"

namespace pierre {
namespace lightdesk {

Dmx::Dmx() {

  DmxClient::setDmx(this);

  _stats.frame.us = _frame_us;
}

Dmx::~Dmx() {}

void Dmx::fpsCalculate(void *data) {
  Dmx_t *dmx = (Dmx_t *)data;
  const auto mark = dmx->_frame_count_mark;
  const auto count = dmx->_stats.frame.count;

  if (mark && count) {
    dmx->_fpcp = count - mark;
    dmx->_stats.fps = (float)dmx->_fpcp / (float)dmx->_fpc_period;
  }

  dmx->_frame_count_mark = count;
}

void Dmx::frameApplyUpdates() {
  // update the next frame with registered head unit staged frame updates

  for (auto i = 0; i < _clients; i++) {
    DmxClient *client = _client[i];

    if (client) {
      client->frameUpdate(_frame);
    }
  }
}

void Dmx::framePrepareCallback() {

  for (auto k = 0; k < _clients; k++) {
    DmxClient *client = _client[k];

    if (client) {
      client->framePrepare();
    }
  }
}

void Dmx::frameTimerCallback(void *data) {
  Dmx_t *dmx = (Dmx_t *)data;

  // dmx->_ftbf.reset();

  //  dmx->taskNotify(NotifyFrame);
}

// int Dmx::frameTimerStart() {
//   esp_err_t rc = _init_rc;
//
//   if ((_init_rc == ESP_OK) && (_mode == STREAM_FRAMES)) {
//     _stats.frame.white_space_us.track();
//
//     // subtract the elapsed microsecs from when the frame timer fired
//     // measured by _ftbf (frame timer between frames)
//     _ftbf.freeze();
//     rc = esp_timer_start_once(_frame_timer, _frame_us - (uint32_t)_ftbf);
//
//     // notify the task responsible for preparing the next frame
//     framePrepareTaskNotify();
//   }
//
//   return rc;
// }

void Dmx::taskCore(void *task_instance) {
  Dmx_t *dmx = (Dmx_t *)task_instance;

  // dmx->taskInit();
  // dmx->taskLoop(); // returns when _mode == SHUTDOWN
  //
  // // the task is complete, clean up
  // TaskHandle_t task = dmx->_task.handle;
  // dmx->_task.handle = nullptr;
  //
  // // printf("Dmx task %p ending\n", task);
  //
  // vTaskDelete(task); // task is removed the scheduler
}

void Dmx::taskInit() {
  // esp_timer_create_args_t timer_args = {};
  // timer_args.callback = fpsCalculate;
  // timer_args.arg = this;
  // timer_args.dispatch_method = ESP_TIMER_TASK;
  // timer_args.name = "dmx_fps";
  //
  // _init_rc = esp_timer_create(&timer_args, &_fps_timer);
  //
  // if (_init_rc == ESP_OK) {
  //   // the interval to count frames in µs
  //   // _fpc_period is in seconds
  //   _init_rc = esp_timer_start_periodic(_fps_timer, _fpc_period * 1000 *
  //   1000);
  // }
  //
  // if (_init_rc == ESP_OK) {
  //   timer_args.callback = frameTimerCallback;
  //   timer_args.name = "dmx frame";
  //
  //   _init_rc = esp_timer_create(&timer_args, &_frame_timer);
  // }
}

void Dmx::taskLoop() {
  _mode = STREAM_FRAMES;
  // frameTimerStart();

  // when _mode is SHUTDOWN this function returns
  while (_mode != SHUTDOWN) {
    // uint32_t val;
    // auto notified =
    //     xTaskNotifyWait(0x00, ULONG_MAX, &val, pdMS_TO_TICKS(portMAX_DELAY));
    //
    // if (notified == pdPASS) {
    //   const NotifyVal_t notify_val = static_cast<NotifyVal_t>(val);
    //
    //   switch (notify_val) {
    //   case NotifyFrame:
    //     if (_mode == STREAM_FRAMES) {
    //       txFrame();
    //     }
    //     break;
    //
    //   case NotifyStop:
    //     _mode = STOP;
    //     taskNotify(NotifyShutdown);
    //     break;
    //
    //   case NotifyStreamFrames:
    //     _mode = STREAM_FRAMES;
    //     frameTimerStart();
    //     break;
    //
    //   case NotifyShutdown:
    //     _mode = SHUTDOWN;
    //     // wait at least _frame_us to ensure there aren't any pending frame
    //     // timers
    //     vTaskDelay(pdMS_TO_TICKS(1));
    //
    //     if (uart_is_driver_installed(_uart_num)) {
    //       uart_driver_delete(_uart_num);
    //
    //       vTaskDelay(pdMS_TO_TICKS(100));
    //     }
    //
    //     esp_timer_delete(_fps_timer);
    //     _fps_timer = nullptr;
    //
    //     break;
    //
    //   default:
    //     break;
    //   }
    // }
  }
}

// BaseType_t Dmx::taskNotify(NotifyVal_t nval) {
//   const uint32_t val = static_cast<uint32_t>(nval);
//   auto rc = true;
//
//   if (task()) {
//     rc = xTaskNotify(task(), val, eSetValueWithOverwrite);
//   } else {
//     rc = false;
//   }
//   return rc;
// }

void Dmx::taskStart() {
  // if (_task.handle == nullptr) {
  //   // this (object) is passed as the data to the task creation and is
  //   // used by the static coreTask method to call cpre()
  //   ::xTaskCreate(&taskCore, "Rdmx", _task.stackSize, this, _task.priority,
  //                 &(_task.handle));
  // }
}

int Dmx::txBytes() {

  //  frameTimerStart();

  // at the end of the TX the UART pulls the TX low to generate the BREAK
  // once the code reaches this point the BREAK is complete
  // auto bytes = uart_write_bytes_with_break(_uart_num, (const char *)_frame,
  //                                          _frame_len, _frame_break);
  //
  // _stats.tx_ms.track(e.asMillis());

  auto bytes = 0;

  if (bytes == _frame_len) {
    _stats.frame.count++;
  } else {
    _stats.frame.shorts++;
  }

  return bytes;
}

void Dmx::txFrame() {
  // wait up to the max time to transmit a TX frame
  // const TickType_t uart_wait_ms = (_frame_us / 1000) + 1;
  // TickType_t frame_ticks = pdMS_TO_TICKS(uart_wait_ms);
  //
  // // always ensure the previous tx has completed which includes
  // // the BREAK (low for 88us)
  // if (uart_wait_tx_done(_uart_num, frame_ticks) == ESP_OK) {
  //   _stats.frame.white_space_us.record();
  //
  //   txBytes();
  //   frameApplyUpdates();
  // }
}

// esp_err_t Dmx::uartInit() {
//   esp_err_t esp_rc = ESP_FAIL;
//
//   if (_init_rc == ESP_OK) {
//     uart_config_t uart_conf = {};
//     uart_conf.baud_rate = 250000;
//     uart_conf.data_bits = UART_DATA_8_BITS;
//     uart_conf.parity = UART_PARITY_DISABLE;
//     uart_conf.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
//     uart_conf.source_clk = UART_SCLK_APB;
//     uart_conf.stop_bits = UART_STOP_BITS_2;
//
//     if ((esp_rc = uart_param_config(_uart_num, &uart_conf)) != ESP_OK) {
//       ESP_LOGW(pcTaskGetTaskName(nullptr), "[%s] uart_param_config()",
//                esp_err_to_name(esp_rc));
//     };
//
//     uart_set_pin(_uart_num, _tx_pin, 16, UART_PIN_NO_CHANGE,
//                  UART_PIN_NO_CHANGE);
//
//     // this sequence is not part of the DMX512 protocol.  rather, these bytes
//     // are sent to identify initiialization when viewing the serial data on
//     // an oscillioscope.
//     const char init_bytes[] = {0xAA, 0x55, 0xAA, 0x55};
//     const size_t len = sizeof(init_bytes);
//     uart_write_bytes_with_break(_uart_num, init_bytes, len, (_frame_break *
//     2));
//   }
//
//   return esp_rc;
// }

void Dmx::waitForStop() {
  // bool uart_installed = true;
  //
  // do {
  //   // delay until the uart is uninstalled
  //   if (uart_installed) {
  //     vTaskDelay(pdMS_TO_TICKS(10));
  //   }
  //
  //   uart_installed = uart_is_driver_installed(_uart_num);
  //
  // } while (uart_installed);
}

//
// DMX Client
//

void DmxClient::frameUpdate(uint8_t *frame_actual) {
  if (_frame_changed) {

    // DmxClient is subclassed for multiple purposes:
    //
    // 1. when frame_len > 0 the state must be included in the frame
    // 2. when frame_len == 0 the subclass is using Dmx as the clock
    //    to perform it's own internal non-DMX updates
    //
    if (_frame_len > 0) {
      // bcopy(_frame_snippet, (frame_actual + _address), _frame_len);
    }

    _frame_changed = false;
  }
}
} // namespace lightdesk
} // namespace pierre
