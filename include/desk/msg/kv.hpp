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

//
// msg keys
//

// common local -> remote
static auto constexpr MAGIC{"ma"};                // msg end detection
static auto constexpr MSG_TYPE{"mt"};             // msg start detection
static auto constexpr NOW_REAL_US{"now_real_µs"}; // all msgs
static auto constexpr NOW_US{"now_µs"};           // all msgs

// periodic
static constexpr const auto SUPP{"supp"}; // msg contains supplemental metrics

// handshake local -> remote
static auto constexpr DATA_PORT{"data_port"}; // handshake
static auto constexpr FRAME_LEN{"frame_len"}; // handshake
static auto constexpr FRAME_US{"frame_µs"};   // handshake
static auto constexpr IDLE_MS{"idle_ms"};     // handshake
static auto constexpr STATS_MS{"stats_ms"};   // handshake

// data msg local -> remote
static auto constexpr FRAME{"frame"};     // data msg
static auto constexpr SEQ_NUM{"seq_num"}; // data msg
static auto constexpr SILENCE{"silence"}; // data msg

// data msg reply remote -> local
static auto constexpr ECHO_NOW_US{"echo_now_µs"}; // echoed
static auto constexpr ELAPSED_US{"elapsed_µs"};   // data_reply msg
static auto constexpr DATA_WAIT_US{"data_wait_µs"};

// stats msg remote -> local
static auto constexpr FPS{"fps"};   // frames per second
static auto constexpr QOK{"qok"};   // frame queue ok
static auto constexpr QRF{"qrf"};   // rame queue recv failure count
static auto constexpr QSF{"qsf"};   // queue send failure count
static auto constexpr UART{"uart"}; // uart overrun (timeout)

//
// msg types
//

static auto constexpr DATA{"data"};
static auto constexpr DATA_REPLY{"data_reply"};
static auto constexpr HANDSHAKE{"handshake"};
static auto constexpr SHUTDOWN{"shutdown"};
static auto constexpr STATS{"stats"};
static auto constexpr UNKNOWN{"unknown"};

//
// msg values
//
static constexpr uint16_t MAGIC_VAL{0x033c};

} // namespace desk
} // namespace pierre