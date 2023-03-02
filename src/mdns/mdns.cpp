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
#include "base/thread_util.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "mdns_ctx.hpp"
#include "service.hpp"
#include "zservice.hpp"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/timeval.h>
#include <thread>

namespace pierre {

namespace shared {
std::unique_ptr<mDNS> mdns;
}

using namespace mdns;

// mDNS general API

mDNS::mDNS() noexcept
    : receiver(config_val<mDNS, string>("receiver", "Pierre Default")), //
      build_vsn(cfg_build_vsn()),                                       //
      stype(config_val<mDNS, string>("service", "_ruth._tcp")),         //
      receiver_port(config_val<mDNS, Port>("port", 7000)),              //
      service_obj(receiver, build_vsn),                                 //
      ctx{nullptr}                                                      //
{
  std::jthread([this]() {
    INFO_INIT("sizeof={:>5} receiver='{}' dmx_service={}\n", sizeof(mDNS), receiver, stype);

    thread_util::set_name(module_id);

    // start avahi cliet
    ctx = std::make_unique<mdns::Ctx>(stype, service_obj, receiver_port);

    if (!ctx->err_msg.empty()) {
      INFO_INIT("FAILED, reason={}\n", ctx->err_msg);
    }
  }).join(); // wait for startup to complete
}

mDNS::~mDNS() noexcept { ctx.reset(); }

void mDNS::browse(csv stype) noexcept { // static
  shared::mdns->ctx->browse(stype);
}

void mDNS::update() noexcept { // static
  shared::mdns->ctx->update(shared::mdns->service_obj);
}

ZeroConfFut mDNS::zservice(csv name) noexcept { // static
  return shared::mdns->ctx->zservice(name);
}

} // namespace pierre