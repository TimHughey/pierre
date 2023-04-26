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
#include "base/conf/fixed.hpp"
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
    : // create our config token (populated)
      conf::token(module_id),
      // get the receiver
      receiver(conf_val<string>("receiver"_tpath, def_receiver.data())),               //
      build_vsn(conf::fixed::git()),                                                   //
      stype(conf_val<string>("service"_tpath, def_stype.data())),                      //
      receiver_port(conf_val<int>("port"_tpath, 7000)),                                //
      service_obj(std::make_unique<Service>(receiver, build_vsn, msgs.emplace_back())) //
{}

mDNS::~mDNS() noexcept { ctx.reset(); }

void mDNS::browse(csv st) noexcept { // static
  shared::mdns->ctx->browse(st);
}

void mDNS::start() noexcept {

  ctx = std::make_unique<mdns::Ctx>(stype, service_obj.get(), receiver_port);
  ctx->run(msgs.emplace_back());

  std::for_each(msgs.begin(), msgs.end(), [this](const auto &m) {
    constexpr auto fn_id{"start"};
    INFO_AUTO("{}\n", m);
  });
}

void mDNS::update() noexcept { // static
  shared::mdns->ctx->update();
}

ZeroConfFut mDNS::zservice(csv name) noexcept { // static
  return shared::mdns->ctx->zservice(name);
}

} // namespace pierre