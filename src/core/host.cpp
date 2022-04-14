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
#include <exception>
#include <filesystem>
#include <fmt/format.h>
#include <gcrypt.h>
#include <ifaddrs.h>
#include <iomanip>
#include <iterator>
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <sodium.h>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <vector>

#include "host.hpp"
#include "pair/pair.h"

namespace pierre {
using std::array, std::string, std::string_view;

Host::Host(const string &firmware_vsn, const string &service_name)
    : _firmware_vsn(firmware_vsn), _service_name(service_name) {
  // find the mac addr, store a binary and text representation
  createHostIdentifiers();

  // create UUID to represent this host
  createUUID();
  createPrivateKey();
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

void Host::createPrivateKey() {
  // initialize crypo libs
  if (sodium_init() < 0) {
    throw(std::runtime_error{"sodium_init() failed"});
  }

  gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
  gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

  auto *dest = _pk_bytes.data();
  auto *secret = hwAddr();

  pair_public_key_get(PAIR_SERVER_HOMEKIT, dest, secret);
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

const string Host::pk(const char *format) const {
  return fmt::format(format, fmt::join(_pk_bytes, ""));
}

} // namespace pierre