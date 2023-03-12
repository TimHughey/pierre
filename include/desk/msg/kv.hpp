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
static constexpr auto NOW_US{"now_µs"}; // all msgs
static constexpr auto REF_US{"ref_µs"}; // all msgs
static constexpr auto MAGIC{"ma"};      // msg end detection
static constexpr auto MSG_TYPE{"mt"};   // msg start detection

// handshake local -> remote
static constexpr auto FRAME_LEN{"frame_len"}; // handshake
static constexpr auto IDLE_MS{"idle_ms"};     // handshake
static constexpr auto STATS_MS{"stats_ms"};   // handshake

// data msg local -> remote
static constexpr auto FRAME{"frame"};     // data msg
static constexpr auto SEQ_NUM{"seq_num"}; // data msg
static constexpr auto SILENCE{"silence"}; // data msg

// data msg reply remote -> local
static constexpr auto ECHO_NOW_US{"echo_now_µs"}; // echoed
static constexpr auto ELAPSED_US{"elapsed_µs"};   // data_reply msg
static constexpr auto DATA_WAIT_US{"data_wait_µs"};

// stats msg remote -> local
static constexpr auto FPS{"fps"};   // frames per second
static constexpr auto QOK{"qok"};   // frame queue ok
static constexpr auto QRF{"qrf"};   // rame queue recv failure count
static constexpr auto QSF{"qsf"};   // queue send failure count
static constexpr auto UART{"uart"}; // uart overrun (timeout)

//
// msg types
//

static constexpr auto DATA{"data"};
static constexpr auto DATA_REPLY{"data_reply"};
static constexpr auto HANDSHAKE{"handshake"};
static constexpr auto SHUTDOWN{"shutdown"};
static constexpr auto STATS{"stats"};
static constexpr auto UNKNOWN{"unknown"};

//
// msg values
//
static constexpr uint16_t MAGIC_VAL{0x033c};

} // namespace desk
} // namespace pierre