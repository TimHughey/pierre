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

#pragma once

#include "base/typical.hpp"

#include <ArduinoJson.h>
#include <array>
#include <memory>
#include <optional>

namespace pierre {

class Config;
typedef std::shared_ptr<Config> shConfig;

namespace shared {
std::optional<shConfig> &config();
} // namespace shared

class Config : public std::enable_shared_from_this<Config> {
private:
  static constexpr auto MAX_DOC_SIZE = 10 * 1024;

public:
  struct Inject {
    string app_name;
    string cli_cfg_file;
    string hostname;
  };

private:
  Config(const Inject &di);

public:
  // create, access shared Config
  static shConfig init(const Inject &di) { return shared::config().emplace(new Config(di)); }
  static shConfig ptr() { return shared::config().value()->shared_from_this(); }
  static void reset() { shared::config().reset(); }

  // general API
  static csr appName() { return ptr()->di.app_name; }
  static csr firmwareVersion() { return ptr()->firmware_vsn; };
  static csv moduleID() { return module_id; }
  static JsonObject object(csv key) { return ptr()->doc[key]; }
  static const string receiverName() { return ptr()->receiver(); }

  void test(const char *setting, const char *key);

private:
  const string receiver() const {
    if (csv r = doc["pierre"]["receiver_name"]; r.size()) {
      if (r == csv("%h")) {
        return string(di.hostname);
      } else {
        return string(r);
      }
    }

    return string(di.hostname);
  }

private:
  // order dependent (initialized by constructor)
  Inject di;
  string firmware_vsn;

  // order independent
  string cfg_file; // will be an empty string if no on disk file was located

  // doc is guarenteed to be valid (either from a file on disk or the fallback config)
  StaticJsonDocument<MAX_DOC_SIZE> doc;

  static constexpr csv module_id = "PE_CONFIG";
};

} // namespace pierre