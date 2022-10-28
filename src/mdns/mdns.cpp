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
#include "base/logger.hpp"
#include "core/service.hpp"
#include "provider.hpp"
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
std::optional<mDNS> mdns;
} // namespace shared

// mDNS Class General API

void mDNS::browse(csv stype) { // static
  shared::mdns->browse_impl(stype);
}

void mDNS::browse_impl(csv stype) { // static
  [[maybe_unused]] static constexpr csv fn_id{"BROWSE"};

  if (auto sb = avahi_service_browser_new(mdns::_client,       // client
                                          AVAHI_IF_UNSPEC,     // network interface
                                          AVAHI_PROTO_UNSPEC,  // any protocol
                                          stype.data(),        // service type
                                          nullptr,             // domain
                                          (AvahiLookupFlags)0, // lookup flags
                                          mdns::cb_browse,     // callback
                                          mdns::_client);      // userdata
      sb == nullptr) {
    INFO(module_id, fn_id, "create failed reason={}\n", mdns::error_string(mdns::_client));
  } else {
    INFO(module_id, fn_id, "initiated browse for stype={}\n", stype);
  }
}

void mDNS::init(io_context &io_ctx) noexcept { // static
  if (shared::mdns.has_value() == false) {
    shared::mdns.emplace(io_ctx).init_self();
  }
}

void mDNS::init_self() {

  mdns::_tpoll = avahi_threaded_poll_new();
  auto poll = avahi_threaded_poll_get(mdns::_tpoll);

  int err;
  if (mdns::_client = avahi_client_new(poll, AVAHI_CLIENT_NO_FAIL, mdns::cb_client, nullptr, &err);
      mdns::_client) {

    if (avahi_threaded_poll_start(mdns::_tpoll) < 0) {
      error = "avahi threaded poll failed to start";
    }
  } else {
    error = avahi_strerror(err);
  }

  if (!error.empty()) {
    INFO(module_id, "INIT", "error={}\n", error);
  }
}

void mDNS::update() noexcept { shared::mdns->update_impl(); };

void mDNS::update_impl() {
  [[maybe_unused]] static constexpr csv fn_id = "UPDATE";
  INFOX(mDNS::module_id, fn_id, "groups={}\n", mdns::_groups.size());

  avahi_threaded_poll_lock(mdns::_tpoll); // lock the thread poll

  AvahiStringList *sl = avahi_string_list_new("autoUpdate=true", nullptr);

  const auto kvmap = Service::ptr()->keyValList(service::Type::AirPlayTCP);
  for (const auto &[key, val] : *kvmap) {
    INFOX(mDNS::module_id, fn_id, "{:>18}={}\n", key, val);

    sl = avahi_string_list_add_pair(sl, key, val);
  }

  const auto [reg_type, name] = Service::ptr()->nameAndReg(service::Type::AirPlayTCP);
  auto group = mdns::_groups.front(); // update the first (and only) group

  if (auto rc =                                    //
      avahi_entry_group_update_service_txt_strlst( // update the group
          group,                                   //
          AVAHI_IF_UNSPEC,                         //
          AVAHI_PROTO_UNSPEC,                      //
          (AvahiPublishFlags)0,                    //
          name,                                    //
          reg_type,                                //
          nullptr,                                 //
          sl                                       // string list to apply to the group
      );
      rc != 0) {
    INFO(module_id, fn_id, "failed rc={}\n", rc);
  }

  avahi_threaded_poll_unlock(mdns::_tpoll); // resume thread poll
  avahi_string_list_free(sl);
}

void mDNS::reset() { shared::mdns.reset(); }

std::future<ZeroConf> mDNS::zservice(csv name) {
  ZeroConfProm prom;
  ZeroConfFut future = prom.get_future();

  avahi_threaded_poll_lock(mdns::_tpoll);

  auto cmp = [=](const auto &zs) { return zs.match_name(name); };

  if (auto it = ranges::find_if(mdns::zcs_list, cmp); it != mdns::zcs_list.end()) {
    INFO(module_id, "ZSERVICE", "matched {}\n", it->inspect());
    prom.set_value(*it);
  } else {

    auto [it_known, inserted] = mdns::zcs_proms.try_emplace(string(name.data()), std::move(prom));

    if (inserted) {
      INFO(module_id, "ZSERVICE", "stored promise for {}\n", name);
    }
  }

  avahi_threaded_poll_unlock(mdns::_tpoll);

  return future;
}

} // namespace pierre