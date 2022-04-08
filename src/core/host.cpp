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

#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/format.h>
#include <ifaddrs.h>
#include <iomanip>
#include <iterator>
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <vector>

#include "host.hpp"

namespace pierre {
using std::array, std::string, std::string_view;

Host::Host() {
  // find the mac addr, store a binary and text representation
  createHostIdentifiers();

  // create UUID to represent this host
  createUUID();
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

void Host::createUUID() {
  uuid_t binuuid;
  uuid_generate_random(binuuid);

  array<char, 37> tu = {0};
  uuid_unparse_lower(binuuid, tu.data());

  _uuid = string(tu.data(), tu.size());
}

bool Host::findHardwareAddr(HwAddrBytes &dest) {
  auto found = false;
  struct ifaddrs *ifaddr = NULL;
  constexpr auto exclude_lo = "lo";

  if (getifaddrs(&ifaddr) == -1) {
    return found;
  }

  for (auto ifa = ifaddr; (ifa != NULL) && !found; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr->sa_family == AF_PACKET) {
      if (strcmp(ifa->ifa_name, exclude_lo) != 0) {
        auto *s = (struct sockaddr_ll *)ifa->ifa_addr;

        // only copy bytes that fit into the destination
        for (auto idx = 0; idx < dest.size(); idx++) {
          dest.at(idx) = s->sll_addr[idx];
        }

        found = true;
      }
    }
  }

  freeifaddrs(ifaddr);
  return found;
}

} // namespace pierre