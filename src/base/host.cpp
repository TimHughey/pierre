/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "host.hpp"

#include <arpa/inet.h>
#include <array>
#include <cstdlib>
#include <exception>
#include <fmt/format.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> // the L2 protocols
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace pierre {

Host::Host() noexcept : name(255, 0x00) {
  discover_ip_addrs();

  // create the various representations of this host
  if (_hw_addr_bytes[0] != 0x00) {
    // build the various representations of hw address needed

    // device_id is the hwaddr bytes concatenated
    id = fmt::format("{:02X}", fmt::join(_hw_addr_bytes, ""));

    // hw_addr is the traditional colon separated hex bytes
    hw_addr = fmt::format("{:02X}", fmt::join(_hw_addr_bytes, ":"));

    // serial num is the dash separated hex bytes with a trailing colon + number
    serial = fmt::format("{:02X}:5", fmt::join(_hw_addr_bytes, "-"));
  }

  if (gethostname(name.data(), name.size()) != 0) {
    // gethostname() has failed, fallback to device_id
    name = id;
  }
}

void Host::discover_ip_addrs() {
  struct ifaddrs *addrs, *iap;

  if (getifaddrs(&addrs) < 0) {
    throw(std::runtime_error("Host getifaddrs() failed"));
  }

  for (iap = addrs; iap != NULL; iap = iap->ifa_next) {

    if (iap->ifa_addr                              // an actual address
        && iap->ifa_netmask                        // non-zero netmask
        && (iap->ifa_flags & IFF_UP)               // iterface is up
        && ((iap->ifa_flags & IFF_LOOPBACK) == 0)) // not loopback
    {                                              // consider this address

      // use IPv6 length (for possible support of ipv6)
      std::array<char, INET6_ADDRSTRLEN + 1> buf{0}; // zero the buffer

      // for troubleshooting, ignore ipv6 - 2022-05-24
      if (iap->ifa_addr->sa_family == AF_INET6) {
        continue;
        // struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)(iap->ifa_addr);
        // inet_ntop(AF_INET6, (void *)&addr6->sin6_addr, buf.data(), buf.size());
      } else {
        struct sockaddr_in *addr = (struct sockaddr_in *)(iap->ifa_addr);
        inet_ntop(AF_INET, (void *)&addr->sin_addr, buf.data(), buf.size());
      }

      if (buf[0] != 0) {
        ip_addrs.emplace_back(buf.data());

        // grab the interface mac addr if we don't have one yet
        if (_hw_addr_bytes[0] == 0x00) {
          // auto *s = (struct sockaddr_ll *)iap->ifa_addr;

          std::memcpy(_hw_addr_bytes.data(), iap->ifa_addr, _hw_addr_bytes.size());
        }
      }
    }
  }

  freeifaddrs(addrs);
}

} // namespace pierre