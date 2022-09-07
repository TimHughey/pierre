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

#include "base/clock_info.hpp"
#include "base/pet.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "io/io.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <pthread.h>
#include <vector>

namespace pierre {

struct ClockPort {
  string id;
  Port port;
};

typedef std::vector<ClockPort> ClockPorts;

struct PeerInfo {
  string id;
  uint8v addresses;
  ClockPorts clock_ports;
  int device_type = 0;
  ClockID clock_id = 0;
  bool port_matching_override = false;
};

typedef std::vector<PeerInfo> PeerList;

class MasterClock;
namespace shared {
extern std::optional<MasterClock> master_clock;
}

class MasterClock {
private:
  static constexpr uint16_t CTRL_PORT = 9000; // see note
  static constexpr auto LOCALHOST = "127.0.0.1";
  static constexpr csv module_id{"MASTER CLOCK"};
  static constexpr uint16_t NQPTP_VERSION = 7;

public:
  typedef std::vector<string> Peers;

public:
  struct Inject {
    io_context &io_ctx;
    csv service_name;
    csv device_id;
  };

public:
  struct nqptp {
    pthread_mutex_t copy_mutes;           // for safely accessing the structure
    uint16_t version;                     // check version==VERSION
    uint64_t master_clock_id;             // the current master clock
    char master_clock_ip[64];             // where it's coming from
    uint64_t local_time;                  // the time when the offset was calculated
    uint64_t local_to_master_time_offset; // add this to the local time to get master clock time
    uint64_t master_clock_start_time;     // this is when the master clock became master}
  };

public:
  MasterClock(const Inject &di);
  // prevent copies, moves and assignments
  MasterClock(const MasterClock &clock) = delete;            // no copy
  MasterClock(const MasterClock &&clock) = delete;           // no move
  MasterClock &operator=(const MasterClock &clock) = delete; // no copy assignment
  MasterClock &operator=(MasterClock &&clock) = delete;      // no move assignment

  ~MasterClock() { unMap(); }

  static MasterClock &init(const Inject &inject) { return shared::master_clock.emplace(inject); }
  static void reset() { shared::master_clock.reset(); }

  // NOTE: new info is updated every 126ms
  const ClockInfo info();
  bool ok() { return info().ok(); };

  void peersReset() { peersUpdate(Peers()); }
  void peers(const Peers &peer_list) { peersUpdate(peer_list); }

  void teardown() { peersReset(); }

  // misc debug
  void dump() { __LOG0(LCOL01 " inspect info\n{}\n", module_id, csv("DUMP"), info().inspect()); }

private:
  bool isMapped() const;
  bool mapSharedMem();
  void unMap();
  void peersUpdate(const Peers &peers);

private:
  // order dependent
  strand local_strand;
  udp_socket socket;
  ip_address address;
  udp_endpoint endpoint; // local endpoint (c)
  string shm_name;       // shared memmory segment name (built by constructor)

  void *mapped = nullptr; // mmapped region of nqptp data struct

public:
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

} // namespace pierre
