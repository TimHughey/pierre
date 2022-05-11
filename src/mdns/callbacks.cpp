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

#include "mdns/callbacks.hpp"
#include "mdns.hpp"

#include <fmt/format.h>
#include <mutex>
#include <pthread.h>

namespace pierre {
namespace mdns {

using mDNS = pierre::mDNS;

//
// Definitions
//

void cbClient(AvahiClient *client, AvahiClientState state, void *userdata) {
  static std::once_flag __once_flag;

  std::call_once(__once_flag, [] {
    constexpr auto name = "mDNS";

    pthread_setname_np(pthread_self(), name);
  });

  mDNS *mdns = static_cast<mDNS *>(userdata);

  if (mdns == nullptr) {
    fmt::print("cbClient(): mdns={}\n", fmt::ptr(mdns));
    return;
  }

  mdns->_client = client;

  switch (state) {
    case AVAHI_CLIENT_S_REGISTERING:
      mdns->resetGroupIfNeeded();
      break;

    case AVAHI_CLIENT_S_RUNNING:
      mdns->domain = avahi_client_get_domain_name(mdns->_client);
      mdns->advertise(client);
      break;

    case AVAHI_CLIENT_FAILURE:
      fmt::print("cbClient(): client failure\n");
      break;

    case AVAHI_CLIENT_S_COLLISION:
      fmt::print("cbClient(): client name collision\n");
      break;

    case AVAHI_CLIENT_CONNECTING:
      break;
  }
}

void cbBrowse(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol,
              AvahiBrowserEvent event, const char *name, const char *type, const char *domain,
              [[maybe_unused]] AvahiLookupResultFlags flags, void *userdata) {
  // NOTE:  Called when a service becomes available or is removed from the
  // network

  auto mdns = (mDNS *)userdata;

  switch (event) {
    case AVAHI_BROWSER_FAILURE: {
      constexpr auto f = FMT_STRING("browser failure: {}\n");
      mdns->error = fmt::format(f, error_string(b));

      avahi_threaded_poll_quit(mdns->_tpoll);
    } break;

    case AVAHI_BROWSER_NEW:
      // debug(1, "browse_callback: avahi_service_resolver_new for service '%s' of
      // type '%s' in domain
      // '%s'.", name, type, domain);
      /* We ignore the returned resolver object. In the callback
         function we free it. If the server is terminated before
         the callback function is called the server will free
         the resolver for us. */
      if (!(avahi_service_resolver_new(mdns->_client, interface, protocol, name, type, domain,
                                       AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)9, cbResolve,
                                       userdata))) {
        constexpr auto f = FMT_STRING("{} BROWSER RESOLVE failed, service[{}] {} ");
        mdns->error = fmt::format(f, fnName(), name, error_string(mdns->_client));
      }

      break;

    case AVAHI_BROWSER_REMOVE:
      fmt::print("BROWSER REMOVE: service={} type={} domain={}\n", name, type, domain);
      break;

    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
      // debug(1, "(Browser) %s.", event == AVAHI_BROWSER_CACHE_EXHAUSTED ?
      // "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
      break;
  }
}

void cbEntryGroup(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata) {
  mDNS *mdns = static_cast<mDNS *>(userdata);

  if (mdns == nullptr) {
    fmt::print("cbEntryGroup(): mdns={}\n", fmt::ptr(mdns));
    return;
  }

  mdns->_eg_state = state;

  switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
      // fmt::print("mDNS::cbEntryGroup(): committed group={}\n", fmt::ptr(group));

      mdns->saveGroup(group);
      break;

    case AVAHI_ENTRY_GROUP_COLLISION:
      fmt::print("mDNS::cbEntryGroup(): collision group={}\n", fmt::ptr(group));
      break;

    case AVAHI_ENTRY_GROUP_FAILURE:
      fmt::print("mDNS::cbEntryGroup(): failure group={}\n", fmt::ptr(group));
      break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
      // fmt::print("mDNS::cbEntryGroup(): uncommitted group={}\n", fmt::ptr(group));
      break;

    case AVAHI_ENTRY_GROUP_REGISTERING:
      // fmt::print("mDNS::cbEntryGroup(): registering group={}\n", fmt::ptr(group));
      break;

    default:
      break;
  }
}

void cbResolve(AvahiServiceResolver *r, [[maybe_unused]] AvahiIfIndex interface,
               [[maybe_unused]] AvahiProtocol protocol, AvahiResolverEvent event, const char *name,
               const char *type, const char *domain, [[maybe_unused]] const char *host_name,
               [[maybe_unused]] const AvahiAddress *address, [[maybe_unused]] uint16_t port,
               [[maybe_unused]] AvahiStringList *txt,
               [[maybe_unused]] AvahiLookupResultFlags flags, void *userdata) {
  const string dacp_id{name};
  auto *mdns = (mDNS *)userdata;

  fmt::print("cbResolve(): unexpected call");

  /* Called whenever a service has been resolved successfully or timed out */
  mdns->type = type;
  mdns->domain = domain;
  mdns->host_name = host_name;

  switch (event) {
    case AVAHI_RESOLVER_FAILURE:
      mdns->found = false;
      mdns->error = avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r)));

      // debug(2,
      //       "(Resolver) Failed to resolve service '%s' of type '%s' in domain "
      //       "'%s': %s.",
      //       name, type, domain,
      //       avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
      break;
    case AVAHI_RESOLVER_FOUND: // found
      //    char a[AVAHI_ADDRESS_STR_MAX], *t;
      // debug(3, "resolve_callback: Service '%s' of type '%s' in domain '%s':",
      // name, type, domain);
      fmt::print("cbResolve: found service={} type={} domain={}\n", name, type, domain);

      if (dacp_id.ends_with(mdns->_dacp_id)) {
        mdns->found = true;
      }
      break;
  }

  // debug(1,"service resolver freed by resolve_callback");
  avahi_service_resolver_free(r);
}

string error_string(AvahiClient *client) {
  auto client_errno = avahi_client_errno(client);
  return string(avahi_strerror(client_errno));
}

string error_string(AvahiEntryGroup *group) {
  auto client = avahi_entry_group_get_client(group);
  return error_string(client);
}

string error_string(AvahiServiceBrowser *browser) {
  auto client = avahi_service_browser_get_client(browser);

  return error_string(client);
}

} // namespace mdns
} // namespace pierre