
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

#include <array>
#include <condition_variable>
#include <cstdint>
#include <inttypes.h>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <string>
#include <thread>
#include <vector>

#include "core/service.hpp"
#include "nptp/clock_info.hpp"

namespace pierre {

// forward decl for shared_ptr typedef
class Nptp;

typedef std::shared_ptr<Nptp> sNptp;
typedef std::shared_ptr<std::thread> stNptp;

class Nptp : public std::enable_shared_from_this<Nptp> {
public:
  using string = std::string;

public: // destructor
  ~Nptp() { unMap(); }

public: // object creation and shared_ptr API
  [[nodiscard]] static sNptp create(sService service) {
    // not using std::make_shared; constructor is private
    return sNptp(new Nptp(service));
  }

  sNptp getSelf() { return shared_from_this(); }

public: // public API
  const nptp::ClockInfo getClockInfo();
  void refreshClockInfo(nptp::ClockInfo &clock_info);
  void resetPeerList() { sendCtrlMsg("T"); }
  void sendTimingPeers(const std::vector<std::string> &peers);
  void start();

  auto threadHandle() { return _handle; }

private:
  Nptp(sService service);

  // internal API
  bool isMapped() const;
  void openAndMap();
  void runLoop();
  void sendCtrlMsg(const string msg);
  void unMap();

private:
  static constexpr auto _ctrl_port = 9000;
  // The control port expects a UDP packet with the first space-delimited string
  // being the name of the shared memory interface (SMI) to be used.
  // This allows client applications to have a dedicated named SMI interface
  // with a timing peer list independent of other clients. The name given must
  // be a valid SMI name and must contain no spaces. If the named SMI interface
  // doesn't exist it will be created by NQPTP. The SMI name should be delimited
  // by a space and followed by a command letter. At present, the only command
  // is "T", which must followed by nothing or by a space and a space-delimited
  // list of IPv4 or IPv6 numbers, the whole not to exceed 4096 characters in
  // total. The IPs, if provided, will become the new list of timing peers,
  // replacing any previous list. If the master clock of the new list is the
  // same as that of the old list, the master clock is retained without
  // resynchronisation; this means that non-master devices can be added and
  // removed without disturbing the SMI's existing master clock. If no timing
  // list is provided, the existing timing list is deleted. (In future version
  // of NQPTP the SMI interface may also be deleted at this point.) SMI
  // interfaces are not currently deleted or garbage collected.

  bool _ok = false;

  string _shm_name;

  int _shm_fd;
  void *_mapped = nullptr;
  std::jthread _thread;
  std::jthread::native_handle_type _handle;

  // temporary
  int _prev_state = PTHREAD_CANCEL_DISABLE;
  bool _ready = false;
  std::mutex _mutex;
  std::condition_variable _condv;
};

} // namespace pierre