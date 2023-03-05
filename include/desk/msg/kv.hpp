//  Ruth
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include <cstdint>

namespace pierre {
namespace desk {

// predefined msg keys
static constexpr auto DATA_PORT{"data_port"};
static constexpr auto DATA_WAIT_US{"data_wait_µs"};
static constexpr auto DFRAME{"dframe"};
static constexpr auto DMX_QOK{"dmx_qok"}; // dmx queue operation ok
static constexpr auto DMX_QRF{"dmx_qrf"}; // dmx queue recv failure count
static constexpr auto DMX_QSF{"dmx_qsf"}; // dmx queue send failure count
static constexpr auto ECHO_NOW_US{"echo_now_µs"};
static constexpr auto ELAPSED_US{"elapsed_µs"};
static constexpr auto FEEDBACK{"feedback"};
static constexpr auto FPS{"fps"};
static constexpr auto IDLE_SHUTDOWN_MS{"idle_shutdown_ms"};
static constexpr auto MAGIC{"ma"};    // short for msg end detection
static constexpr auto MSG_TYPE{"mt"}; // short for msg start detection
static constexpr auto NOW_US{"now_µs"};
static constexpr auto REF_US{"ref_µs"};
static constexpr auto SEQ_NUM{"seq_num"};
static constexpr auto SILENCE{"silence"};
static constexpr auto STATS_MS{"stats_ms"};

// predefined msg types
static constexpr auto DATA{"data"};
static constexpr auto HANDSHAKE{"handshake"};
static constexpr auto SHUTDOWN{"shutdown"};
static constexpr auto STATS{"stats"};

// predefined msg values
static constexpr uint16_t MAGIC_VAL{0x033c};

} // namespace desk
} // namespace pierre