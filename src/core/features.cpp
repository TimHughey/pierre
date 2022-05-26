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

#include "features.hpp"

#include <array>

namespace pierre {

using namespace ft;

Features::Features() {
  auto set_bits = std::array{b48TransientPairing, b47PeerManagement,     b46HomeKitPairing,
                             b41_PTPClock,        b40BufferedAudio,      b30UnifiedAdvertisingInfo,
                             b22AudioUnencrypted, b20ReceiveAudioAAC_LC, b19ReceiveAudioALAC,
                             b18ReceiveAudioPCM,  b17AudioMetaTxtDAAP,   b16AudioMetaProgress,
                             b15AudioMetaCovers,  b14MFiSoft_FairPlay,   b09AirPlayAudio};

  for (const auto bit : set_bits) {
    _ap2_default.set(bit);
  }
}

uint64_t Features::ap2_default() const { return _ap2_default.to_ullong(); }

} // namespace pierre