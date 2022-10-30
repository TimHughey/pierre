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

#include "base/types.hpp"
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
#include <future>
#include <list>
#include <map>
#include <optional>

#pragma once

namespace pierre {

namespace mdns { // encapsulates provider (Avahi)
using Groups = std::list<AvahiEntryGroup *>;
using Entries = std::vector<string>;
using ZeroConfServiceList = std::list<ZeroConf>;

// provider globals
extern AvahiClient *_client;
extern AvahiThreadedPoll *_tpoll;
extern AvahiEntryGroupState _eg_state;
extern Groups _groups;

// local forward decls
extern void advertise(AvahiClient *client);

template <typename T> string error_string(T t) {
  if constexpr (std::is_same_v<T, AvahiClient>) {
    return string(avahi_strerror(avahi_client_errno(t)));
  } else if constexpr (std::is_same_v<T, AvahiServiceBrowser>) {
    return error_string(avahi_service_browser_get_client(t));
  } else if constexpr (std::is_same_v<T, AvahiEntryGroup>) {
    return error_string(avahi_entry_group_get_client(t));
  }

  return string("unhandled Avahi type, can not create error string");
}

extern bool group_add_service(AvahiEntryGroup *group, service::Type stype, const Entries &entries);
extern bool group_reset_if_needed();
extern void group_save(AvahiEntryGroup *group);
extern zc::TxtList make_txt_list(AvahiStringList *txt);

extern void cb_browse(AvahiServiceBrowser *b, AvahiIfIndex iface, AvahiProtocol protocol,
                      AvahiBrowserEvent event, ccs name, ccs type, ccs domain,
                      AvahiLookupResultFlags flags, void *d);
extern void cb_client(AvahiClient *client, AvahiClientState state, void *d);
extern void cb_entry_group(AvahiEntryGroup *group, AvahiEntryGroupState state, void *d);
extern void cb_resolve(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol,
                       AvahiResolverEvent event, ccs name, ccs type, ccs domain, ccs host_name,
                       const AvahiAddress *address, uint16_t port, AvahiStringList *txt,
                       AvahiLookupResultFlags flags, void *d);

} // namespace mdns
} // namespace pierre