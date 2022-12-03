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

#include "base/types.hpp"

#include <bitset>

namespace pierre {

class StatusFlags {
private:
  enum bits : size_t {
    ProblemsExist = 0,
    NotYetConfigured, // probably a WAC (wireless accessory control)
    AudioLink,
    PINmode,
    PINmatch,
    SupportsAirPlayFromCloud,
    PasswordNeeded, // need password
    Unknown_b08,
    PairingPIN_aka_OTP,       // need PIN to pair
    Enable_HK_Access_Control, // prevents adding to HomeKit when set
    RemoteControlRelay,       // Shows in logs as relayable
    SilentPrimary,
    TightSyncIsGroupLeader,
    TightSyncBuddyNotReachable,
    IsAppleMusicSubscriber,
    iCloudLibraryIsOn,
    ReceiverSessionIsActive,
    Unknown_b18,
    Unknown_b19
  };

public:
  StatusFlags() noexcept {
    // default is always audio cable attached
    _flags.set(AudioLink);
  }

  StatusFlags &ready() noexcept {
    _flags.set(AudioLink);
    _flags.reset(RemoteControlRelay);
    _flags.reset(ReceiverSessionIsActive);

    return *this;
  }

  StatusFlags &rendering() noexcept {
    _flags.set(AudioLink);
    _flags.set(RemoteControlRelay);
    _flags.set(ReceiverSessionIsActive);

    return *this;
  }

  uint32_t val() const { return _flags.to_ulong(); }

private:
  std::bitset<32> _flags;
};

} // namespace pierre
