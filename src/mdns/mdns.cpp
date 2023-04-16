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
#include "base/conf/token.hpp"
#include "base/conf/toml.hpp"
#include "base/logger.hpp"
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
    :                    // create our config token (populated)
      ctoken(module_id), //
      receiver(ctoken.val<string, toml::table>("receiver"_tpath, def_receiver.data())), //
      build_vsn(ctoken.build_vsn()),                                                    //
      stype(ctoken.val<string, toml::table>("service"_tpath, def_stype.data())),        //
      receiver_port(ctoken.val<int, toml::table>("port"_tpath, 7000)),                  //
      service_obj(receiver, build_vsn),                                                 //
      ctx{std::make_unique<mdns::Ctx>(stype, service_obj, receiver_port)}               //
{}

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