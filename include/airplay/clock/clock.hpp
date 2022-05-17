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

#include "clock/info.hpp"
#include "common/typedefs.hpp"
#include "packet/basic.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>

namespace pierre {
namespace airplay {

namespace clock {
struct Inject {
  io_context &io_ctx;
  csv service_name;
  csv device_id;
};
} // namespace clock

class Clock {
private:
  static constexpr uint16_t CTRL_PORT = 9000; // see note
  static constexpr auto LOCALHOST = "127.0.0.1";

public:
  Clock(const clock::Inject &di);

  const clock::Info info();
  bool isMapped(csrc_loc loc = src_loc::current()) const;

  static uint64_t now() { return SteadyClock::now().time_since_epoch().count(); }

  void peersReset() { peersUpdate(Peers()); }
  void peers(const Peers &peer_list) { peersUpdate(peer_list); }

  [[nodiscard]] bool refresh(); // refresh cached values
  void teardown() { peersReset(); }

  // misc debug
  void dump() const;

private:
  bool ensureConnection();
  void sendCtrlMsg(csr msg); // queues async
  void openAndMap();
  void unMap();

  void peersUpdate(const Peers &peers);

private:
  // order dependent
  io_context &io_ctx;
  udp_socket socket;
  ip_address address;
  udp_endpoint endpoint; // local endpoint (c)

  string shm_name; // shared memmory segment name (built by constructor)

  void *mapped = nullptr; // mmapped region of nqptp data struct
  string peer_list;       // timing peers (update when not empty)

  packet::Basic _wire;

public:
  // prevent copies, moves and assignments
  Clock(const Clock &clock) = delete;            // no copy
  Clock(const Clock &&clock) = delete;           // no move
  Clock &operator=(const Clock &clock) = delete; // no copy assignment
  Clock &operator=(Clock &&clock) = delete;      // no move assignment
};

/*
 The control port expects a UDP packet with the first space-delimited string
 being the name of the shared memory interface (SMI) to be used.
 This allows client applications to have a dedicated named SMI interface
 with a timing peer list independent of other clients. The name given must
 be a valid SMI name and must contain no spaces. If the named SMI interface
 doesn't exist it will be created by NQPTP. The SMI name should be delimited
 by a space and followed by a command letter. At present, the only command
 is "T", which must followed by nothing or by a space and a space-delimited
 list of IPv4 or IPv6 numbers, the whole not to exceed 4096 characters in
 total. The IPs, if provided, will become the new list of timing peers,
 replacing any previous list. If the master clock of the new list is the
 same as that of the old list, the master clock is retained without
 resynchronisation; this means that non-master devices can be added and
 removed without disturbing the SMI's existing master clock. If no timing
 list is provided, the existing timing list is deleted. (In future version
 of NQPTP the SMI interface may also be deleted at this point.) SMI
 interfaces are not currently deleted or garbage collected.
*/

} // namespace airplay
} // namespace pierre