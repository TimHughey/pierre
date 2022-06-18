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

#include "core/typedefs.hpp"
#include "packet/basic.hpp"
#include "rtp_time/rtp_time.hpp"

#include <array>
#include <boost/asio.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pierre {

namespace { // anonymous namespace limits scope to this file
using io_context = boost::asio::io_context;
using io_strand = boost::asio::io_context::strand;
using ip_address = boost::asio::ip::address;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;
} // namespace

namespace rtp_time {
struct Inject {
  io_context &io_ctx;
  csv service_name;
  csv device_id;
};

typedef string MasterIP;
typedef std::vector<string> Peers;

// intentional use of namespaces in this header within the rtp_time namespace
using namespace std::chrono;
using namespace std::chrono_literals;

struct Info {
  rtp_time::ClockID clockID = 0;       // current master clock
  rtp_time::MasterIP masterClockIp{0}; // IP of master clock
  Nanos sampleTime;                    // time when the offset was calculated
  uint64_t rawOffset;                  // master clock time = sampleTime + rawOffset
  Nanos mastershipStartTime;           // when the master clock became master

  uint64_t masterTime(uint64_t ref) const { return ref - rawOffset; }
  Nanos masterFor(Nanos now = rtp_time::nowNanos()) const { return now - mastershipStartTime; }

  bool ok() const;

  bool tooOld() const;
  bool tooYoung() const;

  // misc debug
  void dump() const;
};

} // namespace rtp_time

class MasterClock;
typedef std::shared_ptr<MasterClock> shMasterClock;

class MasterClock : public std::enable_shared_from_this<MasterClock> {
private:
  static constexpr uint16_t CTRL_PORT = 9000; // see note
  static constexpr auto LOCALHOST = "127.0.0.1";

public:
  ~MasterClock() { unMap(); }
  static shMasterClock init(const rtp_time::Inject &inject);
  static shMasterClock ptr();
  static void reset();

  const rtp_time::Info getInfo();

  bool ok();

  void peersReset() { peersUpdate(rtp_time::Peers()); }
  void peers(const rtp_time::Peers &peer_list) { peersUpdate(peer_list); }

  void teardown() { peersReset(); }

  // misc debug
  void dump() const;

private:
  MasterClock(const rtp_time::Inject &di);

  bool isMapped(csrc_loc loc = src_loc::current()) const;
  bool mapSharedMem();
  void unMap();
  void peersUpdate(const rtp_time::Peers &peers);

private:
  // order dependent
  io_strand strand;
  udp_socket socket;
  ip_address address;
  udp_endpoint endpoint; // local endpoint (c)

  string shm_name;        // shared memmory segment name (built by constructor)
  void *mapped = nullptr; // mmapped region of nqptp data struct

public:
  // prevent copies, moves and assignments
  MasterClock(const MasterClock &clock) = delete;            // no copy
  MasterClock(const MasterClock &&clock) = delete;           // no move
  MasterClock &operator=(const MasterClock &clock) = delete; // no copy assignment
  MasterClock &operator=(MasterClock &&clock) = delete;      // no move assignment
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
