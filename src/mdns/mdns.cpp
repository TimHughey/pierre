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

using namespace mdns;

// static class data
std::shared_ptr<mDNS> mDNS::self;

// mDNS general API

mDNS::mDNS() noexcept
    : ctx(std::make_unique<mdns::Ctx>()) //
{}

void mDNS::browse(csv stype) noexcept { // static
  auto s = self->ptr();

  s->ctx->browse(stype);
}

void mDNS::init() noexcept { // static
  if (self.use_count() < 1) {
    self = std::shared_ptr<mDNS>(new mDNS());

    self->service_obj.init();
    auto err_msg = self->ctx->init(self->service_obj);

    if (err_msg.size()) {
      INFO(module_id, "init", "FAILED, reason={}\n", err_msg);
    }
  }
}

void mDNS::shutdown() noexcept {
  self->ctx->shutdown();

  self.reset();
}

void mDNS::update() noexcept { // static
  auto s = self->ptr();

  s->ctx->update(s->service_obj);
}

ZeroConfFut mDNS::zservice(csv name) noexcept { // static
  auto s = self->ptr();

  return s->ctx->zservice(name);
}

} // namespace pierre