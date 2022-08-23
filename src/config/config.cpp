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

#include "config/config.hpp"
#include "embed/embed.hpp"
#include "version.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <ranges>

namespace pierre {

namespace shared {
std::optional<shConfig> __config;
std::optional<shConfig> &config() { return shared::__config; }
} // namespace shared

namespace fs = std::filesystem;

const fs::path ETC{"/etc/pierre"};
const fs::path LOCAL_ETC{"/usr/local/etc/pierre"};
static constexpr csv HOME_CFG_DIR = ".pierre";
static constexpr csv CFG_FILE = "live";
static constexpr csv SUFFIX{".json"};
static string HOME{getenv("HOME")};

typedef std::vector<fs::path> paths;

static fs::path ensureAbsolute(csr file_csr) {
  if (auto f = csv(file_csr); f.ends_with(SUFFIX)) { // must have right suffix
    auto fpath = fs::path(f);

    if (fpath.is_relative()) {
      return fs::path(fs::current_path()).append(file_csr).string();
    } else {
      return fpath.string();
    }
  }

  return string();
}

Config::Config(const Inject &di)
    : di(di),                    // store the injected dependencies
      firmware_vsn(GIT_REVISION) // store the firmvsn (git describe)
{

  std::error_code ec;
  auto cwd = fs::current_path(ec);

  __LOG0(LCOL01 " cwd={} reason={}\n", moduleID(), "CONSTRUCT", cwd.c_str(), ec.message());

  const auto file = string(CFG_FILE).append(SUFFIX);

  paths search{
      fs::path(HOME).append(HOME_CFG_DIR).append(file), // 1. home
      fs::path(LOCAL_ETC).append(file),                 // 2. /usr/local/etc
      fs::path(ETC).append(file)                        // 3. /etc
  };

  // was the config file specified via the cli?  if so, add to front of list
  if (auto path = ensureAbsolute(di.cli_cfg_file); !path.empty()) {
    search.emplace(search.begin(), path);
  }

  auto exists = [](fs::path &ptf) {
    auto rc = false;
    std::error_code ec;

    if (const auto stat = fs::status(ptf, ec); !ec) {
      rc = fs::exists(stat);
    }

    __LOGX(LCOL01 " {} {}\n", moduleID(), rc ? csv("FOUND") : csv("NOT FOUND"), ptf.c_str(),
           ec ? ec.message() : csv(""));

    return rc;
  };

  if (auto found = ranges::find_if(search, exists); found != search.end()) {
    [[maybe_unused]] csv category = "DESERIALIZE";
    cfg_file = found->string();
    std::ifstream cfg_fstream(cfg_file);

    if (cfg_fstream.is_open()) {
      if (auto err = deserializeJson(doc, cfg_fstream); err) {
        __LOGX(LCOL01 " failed reason={}\n", moduleID(), category, err.c_str());

        doc.clear();
      }
    }
  }

  if (doc.isNull()) {
    __LOGX(LCOL01 " using fallback config\n", moduleID(), csv("CONSTRUCT"));
    if (auto err = deserializeJson(doc, cfg::fallback()); err) {
      __LOG0(LCOL01 " err={}\n", moduleID(), csv("FALLBACK"), err.c_str());
    }
  }

  receiver_name = receiver();

  __LOGX(LCOL01 " doc_used={}\n", moduleID(), csv("CONSTRUCT"), doc.memoryUsage());
}

void Config::test(const char *setting, const char *key) {
  JsonObject root = doc.as<JsonObject>();
  const char *val = root[setting][key];

  __LOG0(LCOL01 " setting={} key={} val={}\n", moduleID(), csv("TEST"), //
         setting, key, val);
}

} // namespace pierre