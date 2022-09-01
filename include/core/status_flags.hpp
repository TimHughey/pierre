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

#pragma once

#include "base/typical.hpp"

#include <bitset>

#pragma once

namespace pierre {
namespace sf {

// credit: https://github.com/openairplay/airplay2-receiver
constexpr size_t ProblemsExist = 0;
constexpr size_t NotYetConfigured = 1; // Probably a WAC (wireless accessory ctrl) thing
constexpr size_t AudioLink = 2;        // Audio cable attached (legacy): all is well.
constexpr size_t PINmode = 3;
constexpr size_t PINentry = 4;
constexpr size_t PINmatch = 5;
constexpr size_t SupportsAirPlayFromCloud = 6;
constexpr size_t PasswordNeeded = 7; //  Need password to use
constexpr size_t Unknown_b08 = 8;
constexpr size_t PairingPIN_aka_OTP = 9;        // need PIN to pair
constexpr size_t Enable_HK_Access_Control = 10; // prevents adding to HomeKit when set.
constexpr size_t RemoteControlRelay = 11;       // Shows in logs as relayable
constexpr size_t SilentPrimary = 12;
constexpr size_t TightSyncIsGroupLeader = 13;
constexpr size_t TightSyncBuddyNotReachable = 14;
constexpr size_t IsAppleMusicSubscriber = 15;
constexpr size_t iCloudLibraryIsOn = 16;
constexpr size_t ReceiverSessionIsActive = 17;
constexpr size_t Unknown_b18 = 18;
constexpr size_t Unknown_b19 = 19;

typedef std::bitset<32> Bits;
} // namespace sf

class StatusFlags {
public:
  StatusFlags();

  StatusFlags &ready();
  StatusFlags &rendering();
  uint32_t val() const { return _flags.to_ulong(); }

private:
  sf::Bits _flags;
};

} // namespace pierre
