/*
Pierre - Custom audio processing for light shows at Wiss Landing
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
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "base/typical.hpp"

#include <memory>

namespace pierre {

class mDNS;
typedef std::shared_ptr<mDNS> shmDNS;

class mDNS : public std::enable_shared_from_this<mDNS> {

private:
  mDNS() = default;

public:
  static shmDNS init();
  static shmDNS ptr();
  static void reset();

public:
  static void browse(csv stype);
  static auto port() { return PORT; }
  bool start();
  void update();

  // misc
  static csv moduleID() { return module_id; }

public:
  string _dacp_id;

  bool found = false;
  string type;
  string domain;
  string host_name;
  string error;

private:
  string _service_base;

  static constexpr csv module_id = "mDNS";
  static constexpr auto PORT = 7000;
};

} // namespace pierre