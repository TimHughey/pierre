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

#include "mdns.hpp"
#include "base/typical.hpp"
#include "core/host.hpp"
#include "core/service.hpp"

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
#include <stdlib.h>
#include <vector>

namespace pierre {

namespace shared {
std::optional<shmDNS> __mdns;
std::optional<shmDNS> &mdns() { return shared::__mdns; }
} // namespace shared

// create and access shared MDNS
shmDNS mDNS::init() { return shared::mdns().emplace(new mDNS()); }
shmDNS mDNS::ptr() { return shared::mdns().value()->shared_from_this(); }
void mDNS::reset() { shared::mdns().reset(); }

namespace mdns { // private, hides avahi
typedef std::list<AvahiEntryGroup *> Groups;
typedef std::vector<string> Entries;

// local forward decls
static void advertise(AvahiClient *client);
static bool group_add_service(AvahiEntryGroup *group, service::Type stype, const Entries &entries);
static bool group_reset_if_needed();
static void group_save(AvahiEntryGroup *group);

[[maybe_unused]] static bool resolver_new(AvahiIfIndex interface, AvahiProtocol protocol, ccs name,
                                          ccs type, ccs domain, AvahiLookupFlags flags, void *d);

void cb_browse(AvahiServiceBrowser *b, AvahiIfIndex iface, AvahiProtocol protocol,
               AvahiBrowserEvent event, ccs name, ccs type, ccs domain,
               AvahiLookupResultFlags flags, void *d);
void cb_client(AvahiClient *client, AvahiClientState state, void *d);
void cb_entry_group(AvahiEntryGroup *group, AvahiEntryGroupState state, void *d);
void cb_resolve(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol,
                AvahiResolverEvent event, ccs name, ccs type, ccs domain, ccs host_name,
                const AvahiAddress *address, uint16_t port, AvahiStringList *txt,
                AvahiLookupResultFlags flags, void *d);

static AvahiClient *_client = nullptr;
static AvahiThreadedPoll *_tpoll = nullptr;
static AvahiServiceResolver *_resolver = nullptr;
static AvahiEntryGroupState _eg_state;
static Groups _groups;

// order dependent
string error_string(AvahiClient *client) {
  auto client_errno = avahi_client_errno(client);
  return string(avahi_strerror(client_errno));
}

string error_string(AvahiServiceBrowser *browser) {
  auto client = avahi_service_browser_get_client(browser);
  return error_string(client);
}

string error_string(AvahiEntryGroup *group) {
  auto client = avahi_entry_group_get_client(group);
  return error_string(client);
}

void cb_client(AvahiClient *client, AvahiClientState state, [[maybe_unused]] void *d) {
  static constexpr csv fn_id = "cb_client()";
  static std::once_flag _name_thread_once;

  // name the avahi thread
  std::call_once(_name_thread_once,
                 [self = pthread_self()]() { pthread_setname_np(self, mDNS::moduleID().data()); });

  auto mdns = mDNS::ptr();

  // always perfer the client passed to the callback
  _client = client;

  switch (state) {
  case AVAHI_CLIENT_CONNECTING:
    __LOGX(LCOL01 " connecting\n", mDNS::moduleID(), fn_id);
    break;

  case AVAHI_CLIENT_S_REGISTERING:
    group_reset_if_needed();
    break;

  case AVAHI_CLIENT_S_RUNNING:
    mdns->domain = avahi_client_get_domain_name(_client);
    advertise(client);
    break;

  case AVAHI_CLIENT_FAILURE:
    __LOG0(LCOL01 " client failure reason={}\n", mDNS::moduleID(), fn_id, error_string(_client));
    break;

  case AVAHI_CLIENT_S_COLLISION:
    __LOG0(LCOL01 " client name collison\n", mDNS::moduleID(), fn_id);
    break;
  }
}

void cb_browse([[maybe_unused]] AvahiServiceBrowser *b, [[maybe_unused]] AvahiIfIndex iface,
               [[maybe_unused]] AvahiProtocol protocol, AvahiBrowserEvent event, ccs name,
               ccs type, ccs domain, [[maybe_unused]] AvahiLookupResultFlags flags,
               [[maybe_unused]] void *d) {
  // NOTE:  Called when a service becomes available or is removed from the
  // network

  static constexpr csv fn_id = "cb_browse()";
  auto mdns = mDNS::ptr();

  switch (event) {
  case AVAHI_BROWSER_FAILURE:
    __LOG0(LCOL01 " error={}\n", mDNS::moduleID(), fn_id, error_string(b));
    avahi_threaded_poll_quit(_tpoll);
    break;

  case AVAHI_BROWSER_NEW:
    __LOG0(LCOL01 " NEW host={} type={} domain={}\n", mDNS::moduleID(), fn_id, name, type, domain);

    /* We ignore the returned resolver object. In the callback
          function we free it. If the server is terminated before
          the callback function is called the server will free
          the resolver for us. */

    if (resolver_new(iface,               //
                     protocol,            //
                     name,                //
                     type,                //
                     domain,              // domain
                     (AvahiLookupFlags)0, //
                     d) == false)         // user data
    {

      // if (auto r =                                        //
      //     avahi_service_resolver_new(mdns::_client,       //
      //                                iface,               //
      //                                protocol,            //
      //                                name,                //
      //                                type,                //
      //                                domain,              //
      //                                AVAHI_PROTO_UNSPEC,  //
      //                                (AvahiLookupFlags)0, //
      //                                mdns::cb_resolve,    //
      //                                d);
      //     r == nullptr) {
      __LOG0(LCOL01 " failed to create resolver service={} error={}\n", mDNS::moduleID(),
             fn_id, //
             type, mDNS::ptr()->error);
    }

    break;

  case AVAHI_BROWSER_REMOVE:
    __LOG0(LCOL01 " REMOVE service={} type={} domain={}\n", mDNS::moduleID(), fn_id, //
           name, type, domain);
    break;

  case AVAHI_BROWSER_ALL_FOR_NOW:
    __LOG0(LCOL01 " all for now\n", mDNS::moduleID(), fn_id);
    break;

  case AVAHI_BROWSER_CACHE_EXHAUSTED:
    __LOG0(LCOL01 " cache exhausted\n", mDNS::moduleID(), fn_id);

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

void cb_resolve(AvahiServiceResolver *r, [[maybe_unused]] AvahiIfIndex interface,
                [[maybe_unused]] AvahiProtocol protocol, AvahiResolverEvent event, ccs name,
                ccs type, [[maybe_unused]] ccs domain, [[maybe_unused]] ccs host_name,
                [[maybe_unused]] const AvahiAddress *address, [[maybe_unused]] uint16_t port,
                [[maybe_unused]] AvahiStringList *txt,
                [[maybe_unused]] AvahiLookupResultFlags flags, [[maybe_unused]] void *d) {
  [[maybe_unused]] static constexpr csv fn_id("CB_RESOLVE");

  // const string dacp_id{name};
  auto mdns = mDNS::ptr();

  switch (event) {
  case AVAHI_RESOLVER_FAILURE:
    mdns->found = false;
    mdns->error = error_string(avahi_service_resolver_get_client(r));

    __LOG0(LCOL01 " failed reason={}\n", mDNS::moduleID(), fn_id, mdns->error);

    break;

  case AVAHI_RESOLVER_FOUND: // found
  {
    std::array<char, AVAHI_ADDRESS_STR_MAX> addr_str{0};
    avahi_address_snprint(addr_str.data(), addr_str.size(), address);
    const auto addr_sv = csv(addr_str.data(), addr_str.size());
    auto txt_str = avahi_string_list_to_string(txt);
    auto proto_str = avahi_proto_to_string(protocol);

    __LOG0(LCOL01 " found name={} type={} address={} proto={} flags=0x{:x} txt={}\n", //
           mDNS::moduleID(), fn_id, name, type, addr_sv, proto_str, flags, txt_str);

    avahi_free(txt_str);
  }

  break;
  }

  avahi_service_resolver_free(r);
}

void advertise(AvahiClient *client) {
  [[maybe_unused]] static constexpr csv fn_id("advertise()");

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
      __LOG0(LCOL01 " unhandled error={}\n", mDNS::moduleID(), fn_id, avahi_strerror(rc));
    }
  }
}

bool group_add_service(AvahiEntryGroup *group, service::Type stype, const Entries &entries) {
  [[maybe_unused]] static constexpr csv fn_id = "group_add_service()";
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
    __LOG0(LCOL01 " AirPlay2 name in use\n", mDNS::moduleID(), fn_id);
    avahi_string_list_free(sl);
  } else if (rc != AVAHI_OK) {
    __LOG0(LCOL01 " unhandled error={}\n", mDNS::moduleID(), fn_id, avahi_strerror(rc));
  }

  return (rc == AVAHI_OK) ? true : false;
}

bool group_reset_if_needed() {
  [[maybe_unused]] static constexpr csv fn_id = "group_reset_if_needed()";
  auto rc = true;

  // if there is already a group, reset it
  if (_groups.size()) {
    auto group = _groups.front();

    [[maybe_unused]] const auto state = avahi_entry_group_get_state(group);
    __LOG0(LCOL01 " reset count={} front={} state={}\n", mDNS::moduleID(), fn_id, //
           _groups.size(), fmt::ptr(group), state);

    if (auto ac = avahi_entry_group_reset(group); ac != 0) {
      __LOG0(LCOL01 " failed={}\n", mDNS::moduleID(), fn_id, ac);
      rc = false;
    }
  }

  return rc;
}

void group_save(AvahiEntryGroup *group) {
  [[maybe_unused]] static constexpr csv fn_id = "group_save()";

  if (_groups.size() > 0) {
    [[maybe_unused]] const auto state = avahi_entry_group_get_state(group);
    __LOG0(LCOL01 " exists state-{} same={}\n", mDNS::moduleID(), //
           fn_id, state, (_groups.front() == group));
    return;
  }

  _groups.emplace_back(group);
}

bool resolver_new(AvahiIfIndex interface, AvahiProtocol protocol, ccs name, ccs type, ccs domain,
                  AvahiLookupFlags lookup_flags, void *d) {

  auto rc = true; // assume success
  static constexpr csv fn_id = "resolver_new()";
  if (_resolver = avahi_service_resolver_new(mdns::_client, //
                                             interface,     //
                                             protocol,      //
                                             name, type, domain, AVAHI_PROTO_UNSPEC, lookup_flags,
                                             cb_resolve, d);
      _resolver == nullptr) {
    __LOG0(LCOL01 "failed, service='{}' reason={}\n", mDNS::moduleID(), fn_id, //
           name, mdns::error_string(mdns::_client));
    rc = false;
  }

  return rc;
}

} // namespace mdns

// mDNS Class General API

void mDNS::browse(csv stype) { // static

  [[maybe_unused]] static constexpr csv fn_id = "BROWSE";

  if (auto sb = avahi_service_browser_new(mdns::_client,       //
                                          AVAHI_IF_UNSPEC,     //
                                          AVAHI_PROTO_UNSPEC,  //
                                          stype.data(),        //
                                          nullptr,             // domain
                                          (AvahiLookupFlags)0, // lookup flags
                                          mdns::cb_browse,     // callback
                                          mdns::_client);      // userdata
      sb == nullptr) {
    __LOG0(LCOL01 " create failed reason={}\n", mDNS::moduleID(), fn_id,
           mdns::error_string(mdns::_client));
  }
}

bool mDNS::start() {
  auto rc = true; // assume success
  mdns::_tpoll = avahi_threaded_poll_new();
  auto poll = avahi_threaded_poll_get(mdns::_tpoll);

  int err;
  if (mdns::_client = avahi_client_new(poll, AVAHI_CLIENT_NO_FAIL, mdns::cb_client, nullptr, &err);
      mdns::_client) {

    if (avahi_threaded_poll_start(mdns::_tpoll) < 0) {
      error = "avahi threaded poll failed to start";
      rc = false;
    }
  } else {
    mDNS::ptr()->error = avahi_strerror(err);
    rc = false;
  }

  return rc;
}

void mDNS::update() {
  [[maybe_unused]] static constexpr csv fn_id = "update()";
  __LOGX(LCOL01, " size={}\n", mDNS::moduleID(), fn_id, mdns::_groups.size());

  avahi_threaded_poll_lock(mdns::_tpoll); // lock the thread poll

  AvahiStringList *sl = avahi_string_list_new("autoUpdate=true", nullptr);

  const auto kvmap = Service::ptr()->keyValList(service::Type::AirPlayTCP);
  for (const auto &[key, val] : *kvmap) {
    __LOGX(LCOL01 " {:<18}={}\n", mDNS::moduleID(), fn_id, key, val);

    sl = avahi_string_list_add_pair(sl, key, val);
  }

  constexpr auto iface = AVAHI_IF_UNSPEC;
  constexpr auto proto = AVAHI_PROTO_UNSPEC;
  constexpr auto pub_flags = (AvahiPublishFlags)0;

  const auto [reg_type, name] = Service::ptr()->nameAndReg(service::Type::AirPlayTCP);

  auto group = mdns::_groups.front(); // we're going to update the first (and only group)

  [[maybe_unused]] auto rc = avahi_entry_group_update_service_txt_strlst( //
      group, iface, proto, pub_flags, name, reg_type, nullptr, sl);

  __LOGX(LCOL01 " failed rc={}\n", mDNS::moduleID(), csv("GROUP UPDATE"), rc);

  avahi_threaded_poll_unlock(mdns::_tpoll); // resume thread poll

  avahi_string_list_free(sl);
}

} // namespace pierre