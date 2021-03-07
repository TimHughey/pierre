/*
    local/types.hpp - Ruth Local Types
    Copyright (C) 2017  Tim Hughey

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

#ifndef _pierre_local_types_hpp
#define _pierre_local_types_hpp

#include <memory>
#include <string>

namespace pierre {

typedef std::string string_t;

typedef const char *CSTR; // used for static string assignments
typedef uint32_t Rgbw_t;

typedef enum { BINDER_CLI, BINDER_LIGHTDESK } BinderCategory_t;

typedef enum {
  NotifyZero = 0x0000,
  // timers that most likely firing at or near DMX frame rate
  NotifyTimer = 0x1001,
  NotifyFrame,
  NotifyFaderTimer,
  // notifications for LightDesk commands
  NotifyQueue = 0x2001,
  NotifyColor,
  NotifyDark,
  NotifyDance,
  NotifyFadeTo,
  NotifyMajorPeak,
  // notifications for changing task operational mode and periodic work
  NotifyStop = 0x3001,
  NotifyOff,
  NotifyPause,
  NotifyResume,
  NotifyReady,
  NotifyShutdown,
  NotifyStreamFrames,
  NotifyDelete,
  NotifyStatsCalculate,
  NotifyPrepareFrame,
  // core task notifications
  NotifyTrackHeap = 0x4001,
  NotifyLightDeskController,
  // misc supporting task notifications
  NotifyOtaStart = 0x5001,
  NotifyOtaCancel,
  NotifyOtaFinish,
  // notifications for I2s
  NotifySamplePrint = 0x6001,
  NotifySampleStopPrint
} NotifyVal_t;

} // namespace pierre
#endif // ruth_type_hpp
