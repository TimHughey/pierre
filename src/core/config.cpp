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
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <vector>

#include "config.hpp"
#include "version.h"

static int get_device_id(uint8_t *id, int int_length) {
  int response = 0;
  struct ifaddrs *ifaddr = NULL;
  struct ifaddrs *ifa = NULL;
  int i = 0;
  uint8_t *t = id;
  for (i = 0; i < int_length; i++) {
    *t++ = 0;
  }

  if (getifaddrs(&ifaddr) == -1) {
    response = -1;
  } else {
    t = id;
    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

      if ((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_PACKET)) {
        struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;
        if ((strcmp(ifa->ifa_name, "lo") != 0) && (found == 0)) {
          for (i = 0; ((i < s->sll_halen) && (i < int_length)); i++) {
            *t++ = s->sll_addr[i];
          }
          found = 1;
        }
      }
    }
    freeifaddrs(ifaddr);
  }
  return response;
}

static uint32_t nctohl(const uint8_t *p) {
  // read 4 characters from *p and do ntohl on them
  // this is to avoid possible aliasing violations
  uint32_t holder;
  memcpy(&holder, p, sizeof(holder));
  return ntohl(holder);
}

static uint16_t nctohs(const uint8_t *p) {
  // read 2 characters from *p and do ntohs on them
  // this is to avoid possible aliasing violations
  uint16_t holder;
  memcpy(&holder, p, sizeof(holder));
  return ntohs(holder);
}

static uint64_t nctoh64(const uint8_t *p) {
  uint32_t landing = nctohl(p); // get the high order 32 bits
  uint64_t vl = landing;
  vl = vl << 32;                          // shift them into the correct location
  landing = nctohl(p + sizeof(uint32_t)); // and the low order 32 bits
  uint64_t ul = landing;
  vl = vl + ul;
  return vl;
}

namespace pierre {

using namespace std;
namespace fs = std::filesystem;
using fs::path;

const fs::path etc{"/etc/pierre"};
const fs::path local_etc{"/usr/local/etc/pierre"};
const string_view home_cfg_dir{".pierre"};
const string_view suffix{".conf"};

typedef std::vector<fs::path> paths;

Config::Config() {
  using namespace fmt;

  // create the UUID for AirPlay mDNS registration
  uuid_t binuuid;
  uuid_generate_random(binuuid);
  uuid_unparse_lower(binuuid, airplay_pi.data());

  get_device_id((uint8_t *)&hw_addr, 6);

  // auto temporary_airplay_id = nctoh64(hw_addr) >> 16;

  // char apids[6 * 2 + 5 + 1]; // six pairs of digits, 5 colons and a NUL
  // apids[6 * 2 + 5] = 0;      // NUL termination
  // int i;
  // char hexchar[] = "0123456789abcdef";
  // for (i = 5; i >= 0; i--) {
  //   apids[i * 3 + 1] = hexchar[temporary_airplay_id & 0xF];
  //   temporary_airplay_id = temporary_airplay_id >> 4;
  //   apids[i * 3] = hexchar[temporary_airplay_id & 0xF];
  //   temporary_airplay_id = temporary_airplay_id >> 4;
  //   if (i != 0)
  //     apids[i * 3 - 1] = ':';
  // }

  firmware_version = GIT_REVISION;
  fmt::print("\nPierre {:>25}\n\n", firmware_version);

  constexpr auto format_s = "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}";
  mac_addr = format(format_s, hw_addr[0], hw_addr[1], hw_addr[2], hw_addr[3], hw_addr[4], hw_addr[5]);

  airplay_device_id = mac_addr;

  // std::array _feature_bits{9, 11, 18, 19, 30, 40, 41, 51};
  // for (const auto &bit : _feature_bits) {
  //   airplay_features |= (1 << bit);
  // }
}

bool Config::findFile(const string &file) {

  // must have suffix .conf
  if (!file.ends_with(suffix))
    return false;

  auto rc = false;
  auto const cwd = fs::current_path();

  // create the first check; if relative prepend with cwd
  auto first = path{file};
  if (first.is_relative()) {
    first = path(cwd).append(file);
  }

  const auto user_home = getenv("HOME");

  // absolute paths to check (so they can be directly set to _cfg_file)
  paths search_paths{first, path{user_home}.append(home_cfg_dir).append(file), path{local_etc}.append(file),
                     path(etc).append(file)};

  // lambda to check each path
  auto check = [](fs::path &check_path) {
    error_code ec;
    auto const stat = fs::status(check_path, ec);

    return !ec && fs::exists(stat);
  };

  // search the paths
  auto found = find_if(search_paths.begin(), search_paths.end(), check);

  // store the path to the config file if found
  if (found != search_paths.end()) {
    _cfg_file = found->c_str();
    rc = true;
  }

  return rc;
}

bool Config::load() {

  if (_cfg_file.empty()) {
    return false;
  }

  // Read the file. If there is an error, report it and exit.
  try {
    readFile(_cfg_file);
  } catch (const libconfig::FileIOException &fioex) {

    fmt::print("I/O error while reading file: {}\n", _cfg_file);
    return false;

  } catch (const libconfig::ParseException &pex) {

    fmt::print("Parse error at {}:{}-{}\n", pex.getFile(), pex.getLine(), pex.getError());
    return false;
  }

  return true;
}

void Config::test(const char *setting, const char *key) {
  const auto &root = getRoot();

  const libconfig::Setting &set = root.lookup(setting);

  const char *val;

  if (set.lookupValue(key, val)) {
    fmt::print("found {}.{}:{}\n", setting, key, val);
  }
}

} // namespace pierre