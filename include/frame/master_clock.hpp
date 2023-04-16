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

#include "frame/clock_info.hpp"

#include <array>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/strand.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace pierre {

namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using strand_ioc = asio::strand<asio::io_context::executor_type>;
using ip_address = boost::asio::ip::address;
using ip_udp = boost::asio::ip::udp;
using udp_endpoint = boost::asio::ip::udp::endpoint;
using udp_socket = boost::asio::ip::udp::socket;

class MasterClock {
private:
  static constexpr auto LOCALHOST{"127.0.0.1"};
  static constexpr uint16_t CTRL_PORT{9000}; // see note

public:
public:
  MasterClock(asio::io_context &io_ctx) noexcept;
  ~MasterClock() noexcept;

  const ClockInfo info_no_wait() noexcept { return load_info_from_mapped(); }

  void peers(const Peers &&peers = std::move(Peers())) noexcept;

  // misc debug
  void dump();

private:
  const ClockInfo load_info_from_mapped() noexcept;

private:
  // order dependent
  const ip_address nqptp_addr;
  const udp_endpoint nqptp_rep; // nqptp remote endpoint (rep)
  strand_ioc local_strand;
  udp_socket peer;

  // order independent
  string shm_name;       // shared memmory segment name (built by constructor)
  void *mapped{nullptr}; // mmapped region of nqptp data struct

public:
  static constexpr csv module_id{"frame.clock"};
};

/*  The control port expects a UDP packet with the first space-delimited string
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
    interfaces are not currently deleted or garbage collected. */

} // namespace pierre
