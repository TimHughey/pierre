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
#include "config.hpp"
#include "pair/pair.h"

#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fmt/format.h>
#include <gcrypt.h>
#include <ifaddrs.h>
#include <iomanip>
#include <iterator>
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <net/if.h>
#include <netinet/in.h>
#include <sodium.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <vector>

namespace pierre {

namespace shared {
std::optional<shHost> __host;
std::optional<shHost> &host() { return shared::__host; }
} // namespace shared

Host::Host(const Inject &di) : cfg(di.cfg) {
  initCrypto();

  // find the mac addr, store a binary and text representation
  createHostIdentifiers();

  // create UUID to represent this host
  createUUID();
  createPublicKey();

  discoverIPs();
}

void Host::createHostIdentifiers() {
  if (findHardwareAddr(_hw_addr_bytes)) {
    // build the various representations of hw address needed

    // device_id is the hwaddr bytes concatenated
    _device_id = fmt::format("{:02X}", fmt::join(_hw_addr_bytes, ""));

    // hw_addr is the traditional colon separated hex bytes
    _hw_addr = fmt::format("{:02X}", fmt::join(_hw_addr_bytes, ":"));

    // serial num is the dash separated hex bytes with a trailing colon + number
    _serial_num = fmt::format("{:02X}:5", fmt::join(_hw_addr_bytes, "-"));
  }
}

void Host::createPublicKey() {
  auto *dest = _pk_bytes.data();
  auto *secret = hwAddr();

  pair_public_key_get(PAIR_SERVER_HOMEKIT, dest, secret);
}

void Host::createUUID() {
  uuid_t binuuid;
  uuid_generate_random(binuuid);

  std::array<char, 37> tu = {0};
  uuid_unparse_lower(binuuid, tu.data());

  _uuid = string(tu.data(), tu.size());
}

void Host::discoverIPs() {
  struct ifaddrs *addrs, *iap;

  if (getifaddrs(&addrs) < 0) {
    throw(std::runtime_error("Host getifaddrs() failed"));
  }

  for (iap = addrs; iap != NULL; iap = iap->ifa_next) {
    // debug(1, "Interface index %d, name:
    // \"%s\"",if_nametoindex(iap->ifa_name), iap->ifa_name);
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
        _ip_addrs.emplace_back(IpAddr{buf.data()});
      }
    }
  }
  freeifaddrs(addrs);
}

bool Host::findHardwareAddr(HwAddrBytes &dest) {
  auto found = false;
  struct ifaddrs *ifaddr = NULL;
  constexpr auto exclude_lo = "lo";

  if (getifaddrs(&ifaddr) < 0) {
    return found;
  }

  for (auto ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr->sa_family == AF_PACKET) {
      if (strcmp(ifa->ifa_name, exclude_lo) != 0) {
        auto *s = (struct sockaddr_ll *)ifa->ifa_addr;

        // only copy bytes that fit into the destination
        for (size_t idx = 0; idx < dest.size(); idx++) {
          dest.at(idx) = s->sll_addr[idx];
        }

        found = true;
        break;
      }
    }
  }

  freeifaddrs(ifaddr);
  return found;
}

void Host::initCrypto() {
  // initialize crypo libs
  if (sodium_init() < 0) {
    throw(std::runtime_error{"sodium_init() failed"});
  }

  if (gcry_check_version(_gcrypt_vsn) == nullptr) {
    static string buff;
    fmt::format_to(std::back_inserter(buff), "outdate libcrypt, need {}\n", _gcrypt_vsn);
    const char *msg = buff.data();
    throw(std::runtime_error(msg));
  }

  gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
  gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
}

const string Host::pk() const { return fmt::format("{:02x}", fmt::join(_pk_bytes, "")); }

} // namespace pierre