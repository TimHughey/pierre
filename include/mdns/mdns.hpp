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

#include "base/types.hpp"
#include "mdns/zservice.hpp"

#include <memory>

namespace pierre {

class mDNS;

namespace shared {
extern std::optional<mDNS> mdns;
}

class mDNS {

public:
  mDNS() = default;

public:
  static void init() noexcept;

  static void reset();

public:
  static void browse(csv &&stype) { shared::mdns->impl_browse(std::forward<csv>(stype)); }
  static auto port() { return PORT; }
  bool start();
  static void update() { shared::mdns->impl_update(); };
  static shZeroConfService zservice(csv type);

private:
  void impl_browse(csv stype);
  void impl_update();
  void init_self();

public:
  string _dacp_id;

  bool found = false;
  string type;
  string domain;
  string host_name;
  string error;

private:
  string _service_base;

public:
  static constexpr csv module_id = "mDNS";

private:
  static constexpr auto PORT = 7000;
};

} // namespace pierre