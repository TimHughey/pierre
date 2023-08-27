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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#pragma once

#include "base/types.hpp"

#include <bitset>

#pragma once

namespace pierre {
namespace ft {

// https://emanuelecozzi.net/docs/airplay2/features/
// https://openairplay.github.io/airplay-spec/features.html
// https://nto.github.io/AirPlay.html
// https://github.com/openrtsp/goplay2
constexpr size_t b00Video = 0;
constexpr size_t b01Photo = 1;
constexpr size_t b02VideoFairPlay = 2;
constexpr size_t b03VideoVolumeCtrl = 3;
constexpr size_t b04VideoHTTPLiveStreaming = 4;
constexpr size_t b05Slideshow = 5;
constexpr size_t b06_Unknown = 6;
// 07: seems to need NTP
constexpr size_t b07ScreenMirroring = 7;
constexpr size_t b08ScreenRotate = 8;
// BIT09 is necessary for iPhones/Music: audio
constexpr size_t b09AirPlayAudio = 9;
constexpr size_t b10Unknown = 10;
constexpr size_t b11AudioRedundant = 11;
// Feat12: iTunes4Win ends ANNOUNCE with rsaaeskey, does not attempt FPLY auth.
// also coerces frequent OPTIONS packets (keepalive) from iPhones.
constexpr size_t b12FPSAPv2p5_AES_GCM = 12;
// 13-14 MFi stuff.
constexpr size_t b13MFiHardware = 13;
// Music on iPhones needs this to stream audio
constexpr size_t b14MFiSoft_FairPlay = 14;
// 15-17 not mandatory - faster pairing without
constexpr size_t b15AudioMetaCovers = 15;
constexpr size_t b16AudioMetaProgress = 16;
constexpr size_t b17AudioMetaTxtDAAP = 17;
// macOS needs 18 to pair
constexpr size_t b18ReceiveAudioPCM = 18;
// macOS needs 19
constexpr size_t b19ReceiveAudioALAC = 19;
// iOS needs 20
constexpr size_t b20ReceiveAudioAAC_LC = 20;
constexpr size_t b21Unknown = 21;
// Try 22 without 40 - ANNOUNCE + SDP
constexpr size_t b22AudioUnencrypted = 22;
constexpr size_t b23RSA_Auth = 23;
constexpr size_t b24Unknown = 24;
// Pairing stalls with longer /auth-setup string w/26
// BIT25 seems to require ANNOUNCE
constexpr size_t b25iTunes4WEncryption = 25;
// try BIT26 without BIT40. BIT26 = crypt audio?
// mutex w/BIT22?
constexpr size_t b26Audio_AES_Mfi = 26;
// 27: connects and works OK
constexpr size_t b27LegacyPairing = 27;
constexpr size_t b28_Unknown = 28;
constexpr size_t b29plistMetaData = 29;
constexpr size_t b30UnifiedAdvertisingInfo = 30;
// Bit 31 Reserved     =  //  31
constexpr size_t b32CarPlay = 32;
constexpr size_t b33AirPlayVideoPlayQueue = 33;
constexpr size_t b34AirPlayFromCloud = 34;
constexpr size_t b35TLS_PSK = 35;
constexpr size_t b36_Unknown = 36;
constexpr size_t b37CarPlayControl = 37;
// 38 seems to be implicit with other flags; works with or without 38.
constexpr size_t b38ControlChannelEncrypt = 38;
constexpr size_t b39_Unknown = 39;
// 40 absence: requires ANNOUNCE method
constexpr size_t b40BufferedAudio = 40;
constexpr size_t b41PTPClock = 41;
constexpr size_t b42ScreenMultiCodec = 42;
// 43
constexpr size_t b43SystemPairing = 43;
constexpr size_t b44APValeriaScreenSend = 44;
// 45: macOS wont connect, iOS will, but dies on play.
// 45 || 41; seem mutually exclusive.
// 45 triggers stream type:96 (without ft41, PTP)
constexpr size_t b45_NTPClock = 45;
constexpr size_t b46HomeKitPairing = 46;
// 47: For PTP
constexpr size_t b47PeerManagement = 47;
constexpr size_t b48TransientPairing = 48;
constexpr size_t b49AirPlayVideoV2 = 49;
constexpr size_t b50NowPlayingInfo = 50;
constexpr size_t b51MfiPairSetup = 51;
constexpr size_t b52PeersExtendedMessage = 52;
constexpr size_t b53_Unknown = 53;
constexpr size_t b54SupportsAPSync = 54;
constexpr size_t b55SupportsWoL = 55;
constexpr size_t b56SupportsWoL = 56;
constexpr size_t b57_Unknown = 57;
constexpr size_t b58HangdogRemote = 58;
constexpr size_t b59AudioStreamConnectionSetup = 59;
constexpr size_t b60AudioMediaDataControl = 60;
constexpr size_t b61RFC2198Redundant = 61;
constexpr size_t b62_Unknown = 62;

// BIT51 - macOS sits for a while. Perhaps trying a closed connection port or
// medium?; iOS just fails at Pair-Setup [2/5]

// features is 64-bits and used for both mDNS (advertisement) and plist (RTSP replies)
//  1. least significant 32-bits (with 0x prefix)
//  2. comma seperator
//  3. most significant 32-bits (with 0x prefix)
//
// examples:
//  mDNS  -> 0x1C340405F4A00: features=0x405F4A00,0x1C340
//  plist -> 0x1C340405F4A00: 496155702020608 (signed int)
//
// uint64_t _features_val = 0x1C340445F8A00; // based on Sonos Amp

using Bits = std::bitset<64>;
} // namespace ft

class Features {
public:
  Features();

  uint64_t ap2Default() const;
  uint64_t ap2SetPeersX() const;

private:
  ft::Bits ap2_default;
  ft::Bits ap2_setpeersx;
};

} // namespace pierre
