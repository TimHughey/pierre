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

#include "provider.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"
#include "core/service.hpp"
#include "mdns.hpp"
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
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <ranges>
#include <stdlib.h>
#include <vector>

namespace pierre {

namespace shared {
extern std::optional<mDNS> mdns;
}

namespace mdns { // private, hides avahi

AvahiClient *_client = nullptr;
AvahiThreadedPoll *_tpoll = nullptr;
AvahiEntryGroupState _eg_state;
Groups _groups;

void cb_client(AvahiClient *client, AvahiClientState state, [[maybe_unused]] void *d) {
  static constexpr csv fn_id{"cb_client()"};

  // name the avahi thread
  pthread_setname_np(pthread_self(), mDNS::module_id.data());

  auto &mdns = shared::mdns.value();

  // always perfer the client passed to the callback
  _client = client;

  switch (state) {
  case AVAHI_CLIENT_CONNECTING:
    INFO(mDNS::module_id, fn_id, "connecting, thread={:#x}\n", pthread_self());
    break;

  case AVAHI_CLIENT_S_REGISTERING:
    group_reset_if_needed();
    break;

  case AVAHI_CLIENT_S_RUNNING:
    mdns.domain = avahi_client_get_domain_name(_client);
    advertise(client);
    break;

  case AVAHI_CLIENT_FAILURE:
    INFO(mDNS::module_id, fn_id, "client failure reason={}\n", error_string(_client));
    break;

  case AVAHI_CLIENT_S_COLLISION:
    INFO(mDNS::module_id, fn_id, "client name collison reason={}\n", error_string(_client));
    break;
  }
}

void cb_browse(AvahiServiceBrowser *b,       //
               AvahiIfIndex iface,           //
               AvahiProtocol protocol,       //
               AvahiBrowserEvent event,      //
               ccs name,                     //
               ccs type,                     //
               ccs domain,                   //
               AvahiLookupResultFlags flags, //
               void *d) {

  // called when a service becomes available or is removed from the network

  static constexpr csv fn_id{"CB_BROWSE"};
  auto &mdns = shared::mdns.value();

  switch (event) {
  case AVAHI_BROWSER_FAILURE:
    INFO(mDNS::module_id, fn_id, "browser={} error={}\n", fmt::ptr(b), error_string(b));
    avahi_threaded_poll_quit(_tpoll);
    break;

  case AVAHI_BROWSER_NEW:
    INFOX(mDNS::module_id, fn_id, "NEW host={} type={} domain={}\n", name, type, domain);

    // ignore the returned resolver object. the callback function we free it.
    // if the server is terminated before the callback function is called the server
    // will free the resolver for us.

    if (auto r = avahi_service_resolver_new(mdns::_client,       //
                                            iface,               //
                                            protocol,            //
                                            name,                //
                                            type,                //
                                            domain,              //
                                            AVAHI_PROTO_UNSPEC,  //
                                            (AvahiLookupFlags)0, //
                                            mdns::cb_resolve,    //
                                            d);
        r == nullptr) {
      INFO(mDNS::module_id, fn_id, "resolver={} service={} flags={}\n", fmt::ptr(r), type, flags);
    }

    break;

  case AVAHI_BROWSER_REMOVE:
    mdns.browser_remove(name);
    break;

  case AVAHI_BROWSER_ALL_FOR_NOW:
    mdns.all_for_now();
    break;

  case AVAHI_BROWSER_CACHE_EXHAUSTED:
    mdns.all_for_now(false);
    break;
  }
}

void cb_entry_group(AvahiEntryGroup *group, AvahiEntryGroupState state, [[maybe_unused]] void *d) {
  _eg_state = state;

  switch (state) {
  case AVAHI_ENTRY_GROUP_ESTABLISHED:
    group_save(group);
    break;

  case AVAHI_ENTRY_GROUP_COLLISION:
    break;

  case AVAHI_ENTRY_GROUP_FAILURE:
    break;

  case AVAHI_ENTRY_GROUP_UNCOMMITED:
    break;

  case AVAHI_ENTRY_GROUP_REGISTERING:
    break;

  default:
    break;
  }
}

void cb_resolve(AvahiServiceResolver *r, AvahiIfIndex iface, AvahiProtocol protocol,
                AvahiResolverEvent event, ccs name, ccs type, ccs domain, ccs host_name,
                const AvahiAddress *address, uint16_t port, AvahiStringList *txt,
                AvahiLookupResultFlags flags, [[maybe_unused]] void *d) {
  [[maybe_unused]] static constexpr csv fn_id{"CB_RESOLVE"};

  auto &mdns = shared::mdns.value();

  switch (event) {
  case AVAHI_RESOLVER_FAILURE: {
    if (avahi_client_errno(mdns::_client) == AVAHI_ERR_TIMEOUT) {
      mdns.browser_remove(name);
    }

    mdns.error = error_string(avahi_service_resolver_get_client(r));

    INFO(mDNS::module_id, fn_id, "failed iface={} flags={} reason={}\n", iface, flags, mdns.error);
    break;
  }

  case AVAHI_RESOLVER_FOUND: {
    std::array<char, AVAHI_ADDRESS_STR_MAX> addr_str{0};
    avahi_address_snprint(addr_str.data(), addr_str.size(), address);

    mdns.resolver_found({
        .domain = domain,
        .hostname = host_name,                                                       //
        .name_net = name,                                                            //
        .address = avahi_address_snprint(addr_str.data(), addr_str.size(), address), //
        .type = type,                                                                //
        .port = port,                                                                //
        .protocol = avahi_proto_to_string(protocol),                                 //
        .txt_list = make_txt_list(txt)                                               //
    });
  }

  break;
  }

  if (r) avahi_service_resolver_free(r);
}

void advertise(AvahiClient *client) {
  [[maybe_unused]] static constexpr csv fn_id("ADVERTISE");

  _client = client;

  if (client && _groups.empty()) {
    // advertise the services
    // build and commit a single group containing two services:
    //  _airplay._tcp string list
    //  _raop._tcp string list

    auto group = avahi_entry_group_new(_client, mdns::cb_entry_group, nullptr);

    for (const auto stype : std::array{service::Type::AirPlayTCP, service::Type::RaopTCP}) {
      Entries entries;

      auto kvmap = Service::ptr()->keyValList(stype);

      for (const auto &[key, val] : *kvmap) { // ranges does not support assoc containers
        entries.emplace_back(fmt::format("{}={}", key, val));
      }

      group_add_service(group, stype, entries);
    }

    if (auto rc = avahi_entry_group_commit(group); rc != AVAHI_OK) {
      INFO(mDNS::module_id, fn_id, "unhandled error={}\n", avahi_strerror(rc));
    }
  }
}

bool group_add_service(AvahiEntryGroup *group, service::Type stype, const Entries &entries) {
  [[maybe_unused]] static constexpr csv fn_id{"GROUP_ADD"};
  constexpr auto iface = AVAHI_IF_UNSPEC;
  constexpr auto proto = AVAHI_PROTO_UNSPEC;
  constexpr auto pub_flags = (AvahiPublishFlags)0;

  const auto [reg_type, name] = Service::ptr()->nameAndReg(stype);

  std::vector<ccs> ccs_ptrs;
  for (auto &entry : entries) {
    ccs_ptrs.emplace_back(entry.c_str());
  }

  auto sl = avahi_string_list_new_from_array(ccs_ptrs.data(), ccs_ptrs.size());
  auto rc = avahi_entry_group_add_service_strlst(group, iface, proto, pub_flags, name, reg_type,
                                                 NULL, NULL, mDNS::port(), sl);

  if (rc == AVAHI_ERR_COLLISION) {
    INFO(mDNS::module_id, fn_id, "AirPlay2 name in use, name={}\n", name);
    avahi_string_list_free(sl);
  } else if (rc != AVAHI_OK) {
    INFO(mDNS::module_id, fn_id, "unhandled error={}\n", avahi_strerror(rc));
  }

  return (rc == AVAHI_OK) ? true : false;
}

bool group_reset_if_needed() {
  [[maybe_unused]] static constexpr csv fn_id{"GROUP_RESET"};
  auto rc = true;

  // if there is already a group, reset it
  if (_groups.size()) {
    auto group = _groups.front();

    [[maybe_unused]] const auto state = avahi_entry_group_get_state(group);
    INFO(mDNS::module_id, fn_id, "reset count={} front={} state={}\n", //
         _groups.size(), fmt::ptr(group), state);

    if (auto ac = avahi_entry_group_reset(group); ac != 0) {
      INFO(mDNS::module_id, fn_id, "failed={}\n", ac);
      rc = false;
    }
  }

  return rc;
}

void group_save(AvahiEntryGroup *group) {
  [[maybe_unused]] static constexpr csv fn_id{"GROUP_SAVE"};

  if (_groups.size() > 0) {
    [[maybe_unused]] const auto state = avahi_entry_group_get_state(group);
    INFO(mDNS::module_id, fn_id, "exists state-{} same={}\n", state, (_groups.front() == group));
    return;
  }

  _groups.emplace_back(group);
}

// Private Utility APIs

zc::TxtList make_txt_list(AvahiStringList *txt) {
  zc::TxtList txt_list;

  if (txt != nullptr) {
    for (AvahiStringList *e = txt; e != nullptr; e = avahi_string_list_get_next(e)) {
      char *key;
      char *val;
      size_t val_size;

      if (auto rc = avahi_string_list_get_pair(e, &key, &val, &val_size); rc == 0) {
        txt_list.emplace_back(key, val);

        avahi_free(key);
        avahi_free(val);
      };
    }
  }

  return txt_list;
}

} // namespace mdns
} // namespace pierre