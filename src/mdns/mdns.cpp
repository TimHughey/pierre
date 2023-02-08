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

#include "mdns.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "mdns_ctx.hpp"
#include "service.hpp"
#include "zservice.hpp"

#include <algorithm>
#include <array>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/timeval.h>
#include <list>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <ranges>
#include <stdlib.h>
#include <vector>

namespace pierre {

namespace shared {
std::unique_ptr<mDNS> mdns;
}

using namespace mdns;

// mDNS general API

mDNS::mDNS() noexcept
    : receiver(config_val2<mDNS, string>("receiver", "Pierre Default")),  //
      build_vsn(cfg_build_vsn()),                                         //
      stype(config_val2<mDNS, string>("service", "_ruth._tcp")),          //
      receiver_port(config_val2<mDNS, Port>("port", 7000)),               //
      service_obj(receiver, build_vsn),                                   //
      ctx(std::make_unique<mdns::Ctx>(stype, service_obj, receiver_port)) //
{
  INFO_INIT("sizeof={:>5} receiver='{}' dmx_service={}\n", sizeof(mDNS), receiver, stype);

  if (!ctx->err_msg.empty()) {
    INFO_INIT("FAILED, reason={}\n", ctx->err_msg);
  }
}

mDNS::~mDNS() noexcept { ctx.reset(); }

void mDNS::browse(csv stype) noexcept { // static
  mdns_ctx()->browse(stype);
}

void mDNS::update() noexcept { // static
  mdns_ctx()->update(shared::mdns->service_obj);
}

ZeroConfFut mDNS::zservice(csv name) noexcept { // static
  return mdns_ctx()->zservice(name);
}

} // namespace pierre