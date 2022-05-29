/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

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

#pragma once

namespace pierre {
namespace airplay {
namespace reply {
namespace dk {

constexpr auto ADDRESSES = csv("Addresses");
constexpr auto AUDIO_FORMAT = csv("audioFormat");
constexpr auto AUDIO_MODE = csv("audioMode");
constexpr auto BUFF_SIZE = csv("audioBufferSize");
constexpr auto CLIENT_ID = csv("clientID");
constexpr auto CONTROL_PORT = csv("controlPort");
constexpr auto COMPRESSION_TYPE = csv("ct");
constexpr auto DATA_PORT = csv("dataPort");
constexpr auto EMPTY = csv();
constexpr auto EVENT_PORT = csv("eventPort");
constexpr auto FLUSH_FROM_SEQ = csv("flushFromSeq");
constexpr auto FLUSH_FROM_TS = csv("flushFromTS");
constexpr auto FLUSH_UNTIL_SEQ = csv("flushUntilSeq");
constexpr auto FLUSH_UNTIL_TS = csv("flushUntilTS");
constexpr auto GROUP_LEADER = csv("groupContainsGroupLeader");
constexpr auto GROUP_UUID = csv("groupUUID");
constexpr auto ID = csv("ID");
constexpr auto IDX0 = csv("0");
constexpr auto NET_TIME_FLAGS = csv("networkTimeFlags");
constexpr auto NET_TIME_FRAC = csv("networkTimeFrac");
constexpr auto NET_TIME_SECS = csv("networkTimeSecs");
constexpr auto NET_TIMELINE_ID = csv("networkTimeTimelineID");
constexpr auto QUALIFIER = csv("qualifier");
constexpr auto RATE = csv("rate");
constexpr auto REMOTE_CONTROL = csv("isRemoteControlOnly");
constexpr auto ROOT = csv();
constexpr auto RTP_TIME = csv("rtpTime");
constexpr auto SAMPLE_FRAMES_PER_PACKET = csv("spf");
constexpr auto SHK = csv("shk");
constexpr auto SR = csv("sr");
constexpr auto STREAM_CONN_ID = csv("streamConnectionID");
constexpr auto STREAM_ID = csv("streamID");
constexpr auto STREAMS = csv("streams");
constexpr auto SUPPORTS_DYNAMIC_STREAM_ID = csv("supportsDynamicStreamID");
constexpr auto TIMING_PEER_INFO = csv("timingPeerInfo");
constexpr auto TIMING_PORT = csv("timingPort");
constexpr auto TIMING_PROTOCOL = csv("timingProtocol");
constexpr auto TYPE = csv("type");

} // namespace dk

namespace dv {
constexpr auto PTP = csv("PTP");
constexpr auto NTP = csv("NTP");
constexpr auto NONE = csv("None");
constexpr auto TXT_AIRPLAY = csv("txtAirPlay");
} // namespace dv

} // namespace reply
} // namespace airplay
} // namespace pierre