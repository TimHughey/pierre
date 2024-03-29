// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/types.hpp"
#include "io/io.hpp"
#include "mdns/service.hpp"
#include "mdns/zservice.hpp"

#include <atomic>
#include <future>
#include <list>
#include <map>
#include <memory>

namespace pierre {

class mDNS;

namespace shared {
extern std::unique_ptr<mDNS> mdns;
}

namespace mdns {
class Ctx;
}

class mDNS {

  friend class mdns::Ctx;

public:
  mDNS() noexcept;
  ~mDNS() noexcept;

public:
  static void browse(csv stype) noexcept;

  static Service &service() noexcept { return shared::mdns->service_obj; }

  static void update() noexcept;
  static ZeroConfFut zservice(csv name) noexcept;

public:
  // order dependent
  const string receiver;
  const string build_vsn;
  const string stype;
  const Port receiver_port;
  Service service_obj;
  std::unique_ptr<mdns::Ctx> ctx;

public:
  static constexpr csv module_id{"mdns"};
};

auto inline mdns_ctx() noexcept { return shared::mdns->ctx.get(); }

} // namespace pierre