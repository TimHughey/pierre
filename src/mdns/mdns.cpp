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
#include "config/config.hpp"
#include "provider.hpp"
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
std::optional<mDNS> mdns;
} // namespace shared

// mDNS general API

void mDNS::all_for_now(bool next_val) noexcept {
  auto prev = _all_for_now.test();

  if (prev != next_val) {
    if (next_val == true) _all_for_now.test_and_set();
    if (next_val == false) _all_for_now.clear();
  }

  INFOX(module_id, "ALL_FOR_NOW", "{} => {} zservices={}\n", prev, _all_for_now.test(),
        std::ssize(zcs_map));
}

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
    INFOX(module_id, fn_id, "initiated browse for stype={}\n", stype);
  }
}

void mDNS::browser_remove(const string name) noexcept {
  static constexpr csv cat{"BROWSE_REMOVE"};
  avahi_threaded_poll_lock(mdns::_tpoll);

  if (auto it = zcs_map.find(name); it != zcs_map.end()) {
    INFO(module_id, cat, "removing name={}\n", name);

    zcs_map.erase(it);
  }

  avahi_threaded_poll_unlock(mdns::_tpoll);
}

void mDNS::init() noexcept { // static
  if (shared::mdns.has_value() == false) {
    shared::mdns.emplace().init_self();
  }
}

void mDNS::init_self() {
  service->init();

  const auto qualifier = service->make_txt_string(txt_type::AirPlayTCP);

  mdns::_tpoll = avahi_threaded_poll_new();
  auto poll = avahi_threaded_poll_get(mdns::_tpoll);

  int err;
  if (mdns::_client = avahi_client_new(poll, AVAHI_CLIENT_NO_FAIL, mdns::cb_client, nullptr, &err);
      mdns::_client) {

    if (avahi_threaded_poll_start(mdns::_tpoll) < 0) {
      error = "avahi threaded poll failed to start";
    }
  } else if (err) {
    error = avahi_strerror(err);
  }

  if (error.empty()) {
    string stype = config()->table().at_path("mdns.service"sv).value_or<string>("_ruth._tcp");
    mDNS::browse(stype);
  } else {
    INFO(module_id, "INIT", "error={}\n", error);
  }
}

void mDNS::resolver_found(const ZeroConf::Details zcd) noexcept {
  static constexpr csv cat{"RESOLVE_FOUND"};

  auto [zc_it, inserted] = zcs_map.try_emplace(zcd.name_net, ZeroConf(zcd));
  const auto &zc = zc_it->second;

  INFOX(module_id, cat, "{} {}\n", inserted ? "resolved" : "already know", zc.inspect());

  if (auto it = zcs_proms.find(zc.name_short()); it != zcs_proms.end()) {
    it->second.set_value(zc);
    zcs_proms.erase(it);
  }
}

void mDNS::reset() { shared::mdns.reset(); }

std::shared_ptr<Service> mDNS::service_ptr() noexcept {
  return shared::mdns->service->shared_from_this();
}

void mDNS::update() noexcept { shared::mdns->update_impl(); };

void mDNS::update_impl() {
  static constexpr csv cat{"UPDATE"};
  // INFOX(mDNS::module_id, cat, "groups={}\n", mdns::_groups.size());

  avahi_threaded_poll_lock(mdns::_tpoll); // lock the thread poll

  AvahiStringList *sl = avahi_string_list_new("autoUpdate=true", nullptr);

  const auto entries = service->key_val_for_type(txt_type::AirPlayTCP);

  for (const auto &[key, val] : entries) {
    INFOX(mDNS::module_id, fn_id, "{:>18}={}\n", key, val);

    sl = avahi_string_list_add_pair(sl, key.data(), val.data());
  }

  const auto [reg_type, name] = service->name_and_reg(txt_type::AirPlayTCP);
  auto group = mdns::_groups.front(); // update the first (and only) group

  if (auto rc =                                    //
      avahi_entry_group_update_service_txt_strlst( // update the group
          group,                                   //
          AVAHI_IF_UNSPEC,                         //
          AVAHI_PROTO_UNSPEC,                      //
          (AvahiPublishFlags)0,                    //
          name.data(),                             //
          reg_type.data(),                         //
          nullptr,                                 //
          sl                                       // string list to apply to the group
      );
      rc != 0) {
    INFO(module_id, cat, "failed rc={}\n", rc);
  }

  avahi_threaded_poll_unlock(mdns::_tpoll); // resume thread poll
  avahi_string_list_free(sl);
}

ZeroConfFut mDNS::zservice(csv name) {
  auto &mdns = shared::mdns.value();

  ZeroConfProm prom;
  ZeroConfFut fut{prom.get_future()};

  avahi_threaded_poll_lock(mdns::_tpoll);

  auto cmp = [=](const auto it) { return it.second.match_name(name); };

  if (auto it = ranges::find_if(mdns.zcs_map.begin(), mdns.zcs_map.end(), cmp);
      it != mdns.zcs_map.end()) {
    prom.set_value(it->second);
  } else {

    auto [it_known, found] = mdns.zcs_proms.try_emplace(name.data(), std::move(prom));

    if (!found) {
      INFO(module_id, "ZSERVICE", "failed to find promise for name={}\n", name);
    }
  }

  avahi_threaded_poll_unlock(mdns::_tpoll);

  return fut;
}

} // namespace pierre