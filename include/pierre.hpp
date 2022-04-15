/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#include <memory>
#include <string>
#include <tuple>

#include "core/config.hpp"

namespace pierre {

class Pierre {
  using string = std::string;

public:
  Pierre() = default;
  ~Pierre() = default;

  Pierre(const Pierre &) = delete;
  Pierre &operator=(const Pierre &) = delete;

  const string &appName() const { return _app_name; }

  std::tuple<bool, ArgsMap> prepareToRun(int argc, char *argv[]);
  void run();

private:
  string _app_name;
  bool _daemonize = false;
  string _dmx_host;

  std::shared_ptr<Config> _cfg;
};

} // namespace pierre
