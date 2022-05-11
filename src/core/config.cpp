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

#include "config.hpp"
#include "version.h"

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

namespace pierre {

namespace fs = std::filesystem;
using fs::path;

const fs::path etc{"/etc/pierre"};
const fs::path local_etc{"/usr/local/etc/pierre"};
const string_view home_cfg_dir{".pierre"};
const string_view suffix{".conf"};

typedef std::vector<fs::path> paths;

Config::Config(const Inject &di)
    : _app_name(di.app_name), _cli_cfg_file(di.cli_cfg_file), _firmware_vsn(GIT_REVISION) {}

bool Config::findFile() {
  const auto file = _cli_cfg_file;

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
  paths search_paths{first, path{user_home}.append(home_cfg_dir).append(file),
                     path{local_etc}.append(file), path(etc).append(file)};

  // lambda to check each path
  auto check = [](fs::path &check_path) {
    std::error_code ec;
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