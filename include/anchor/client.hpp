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

#include <boost/asio.hpp>
#include <future>
#include <memory>
#include <source_location>
#include <string>
#include <thread>

#include "packet/basic.hpp"

namespace pierre {
namespace anchor {
namespace client {

using namespace boost::asio;

class Session {
public:
  using udp_endpoint = ip::udp::endpoint;
  using udp_socket = ip::udp::socket;
  using error_code = boost::system::error_code;
  using src_loc = std::source_location;
  using string = std::string;

  typedef const src_loc csrc_loc;
  typedef const char *ccs;

public:
  // create the Session
  Session(io_context &io_ctx)
      : io_ctx(io_ctx), socket(io_ctx), address(ip::make_address(LOCALHOST)),
        endpoint(udp_endpoint(address, CTRL_PORT)) {}

  void asyncConnect();

  void sendCtrlMsg(const string &shm_name, const string &msg); // queues async

  void teardown() {
    [[maybe_unused]] error_code ec;
    socket.cancel(ec);
    socket.shutdown(udp_socket::shutdown_both, ec);
    socket.close(ec);
  }

  ccs fnName(csrc_loc loc = src_loc::current()) const { return loc.function_name(); }

private:
  // order dependent
  io_context &io_ctx;
  udp_socket socket;
  ip::address address;
  udp_endpoint endpoint;

  packet::Basic _wire;

  static constexpr uint16_t CTRL_PORT = 9000; // see note below
  static constexpr auto LOCALHOST = "127.0.0.1";
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

} // namespace client
} // namespace anchor
} // namespace pierre
