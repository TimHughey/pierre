//  Pierre - Custom Light Show for Wiss Landing
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

#include "base/types.hpp"

namespace pierre {
namespace rtsp {

// keys
constexpr csv ADDRESSES{"Addresses"};
constexpr csv AUDIO_FORMAT{"audioFormat"};
constexpr csv AUDIO_MODE{"audioMode"};
constexpr csv BUFF_SIZE{"audioBufferSize"};
constexpr csv CLIENT_ID{"clientID"};
constexpr csv CONTROL_PORT{"controlPort"};
constexpr csv COMPRESSION_TYPE{"ct"};
constexpr csv DATA_PORT{"dataPort"};
constexpr csv EMPTY = csv();
constexpr csv EVENT_PORT{"eventPort"};
constexpr csv FLUSH_FROM_SEQ{"flushFromSeq"};
constexpr csv FLUSH_FROM_TS{"flushFromTS"};
constexpr csv FLUSH_UNTIL_SEQ{"flushUntilSeq"};
constexpr csv FLUSH_UNTIL_TS{"flushUntilTS"};
constexpr csv GROUP_LEADER{"groupContainsGroupLeader"};
constexpr csv GROUP_UUID{"groupUUID"};
constexpr csv ID{"ID"};
constexpr csv IDX0{"0"};
constexpr csv NET_TIME_FLAGS{"networkTimeFlags"};
constexpr csv NET_TIME_FRAC{"networkTimeFrac"};
constexpr csv NET_TIME_SECS{"networkTimeSecs"};
constexpr csv NET_TIMELINE_ID{"networkTimeTimelineID"};
constexpr csv QUALIFIER{"qualifier"};
constexpr csv RATE{"rate"};
constexpr csv REMOTE_CONTROL{"isRemoteControlOnly"};
constexpr csv ROOT = csv();
constexpr csv RTP_TIME{"rtpTime"};
constexpr csv SAMPLE_FRAMES_PER_PACKET{"spf"};
constexpr csv SHK{"shk"};
constexpr csv SR{"sr"};
constexpr csv STREAM_CONN_ID{"streamConnectionID"};
constexpr csv STREAM_ID{"streamID"};
constexpr csv STREAMS{"streams"};
constexpr csv SUPPORTS_DYNAMIC_STREAM_ID{"supportsDynamicStreamID"};
constexpr csv TIMING_PEER_INFO{"timingPeerInfo"};
constexpr csv TIMING_PORT{"timingPort"};
constexpr csv TIMING_PROTOCOL{"timingProtocol"};
constexpr csv TYPE{"type"};

// values
constexpr csv PTP{"PTP"};
constexpr csv NTP{"NTP"};
constexpr csv NONE{"None"};
constexpr csv TXT_AIRPLAY{"txtAirPlay"};

} // namespace rtsp
} // namespace pierre